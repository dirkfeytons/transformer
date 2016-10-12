/*
 * Copyright (c) 2016 Technicolor Delivery Technologies, SAS
 *
 * The source code form of this Transformer component is subject
 * to the terms of the Clear BSD license.
 *
 * You can redistribute it and/or modify it under the terms of the
 * Clear BSD License (http://directory.fsf.org/wiki/License:ClearBSD)
 *
 * See LICENSE file for more details.
 */

#define _GNU_SOURCE  // needed to get O_CLOEXEC defined
#include "libtransformer.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stddef.h>
#include <inttypes.h>
#include <syslog.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/un.h>

#define TF_LOG_CRIT(FMT, ...)     syslog(LOG_CRIT,    "%s%s: " FMT, "[libtransformer] ", __func__, ##__VA_ARGS__)
#define TF_LOG_ERR(FMT, ...)      syslog(LOG_ERR,     "%s%s: " FMT, "[libtransformer] ", __func__, ##__VA_ARGS__)
#define TF_LOG_WARN(FMT, ...)     syslog(LOG_WARNING, "%s%s: " FMT, "[libtransformer] ", __func__, ##__VA_ARGS__)
#ifdef ENABLE_DEBUG
#define TF_LOG_DBG(FMT, ...)      syslog(LOG_DEBUG,   "%s%s: " FMT, "[libtransformer] ", __func__, ##__VA_ARGS__)
#else
#define TF_LOG_DBG(FMT, ...)
#endif

#define TF_RECEIVE_TIMEOUT        60  // seconds
#define TF_TRANSFORMER_ADDRESS    "transformer"
#define TF_ABSTRACT_SUN_LEN       (offsetof(struct sockaddr_un, sun_path) + sizeof(TF_TRANSFORMER_ADDRESS))
#define TF_MAX_MESSAGE_SIZE       (33 * 1024) // Max message size is 33K

/**
 * The different transformer proxy message types. This list needs to be
 * kept in sync with the one found in transformer/msg.lua
 */
typedef enum {
  MSG_UNKNOWN,            // Unknown message; used to indicate empty msg buffer
  MSG_ERROR_RESP,         // Error response
  MSG_GPV_REQ,            // GetParameterValues request
  MSG_GPV_RESP,           // GetParameterValues response
  MSG_SPV_REQ,            // SetParameterValues request
  MSG_SPV_RESP,           // SetParameterValues response
  MSG_APPLY_REQ,          // Apply
  MSG_ADD_REQ,            // AddObject request
  MSG_ADD_RESP,           // AddObject response
  MSG_DEL_REQ,            // DeleteObject request
  MSG_DEL_RESP,           // DeleteObject response
  MSG_GPN_REQ,            // GetParameterNames request
  MSG_GPN_RESP,           // GetParameterNames response
  MSG_RESOLVE_REQ,        // Resolve request
  MSG_RESOLVE_RESP,       // Resolve response
  MSG_SUBSCRIBE_REQ,      // Subscribe request
  MSG_SUBSCRIBE_RESP,     // Subscribe response
  MSG_UNSUBSCRIBE_REQ,    // Unsubscribe request
  MSG_UNSUBSCRIBE_RESP,   // Unsubscribe response
  MSG_EVENT,              // Event
  MSG_GPL_REQ,            // GetParameterList request
  MSG_GPL_RESP,           // GetParameterList response
  MSG_GPC_REQ,            // GetCount request
  MSG_GPC_RESP            // GetCount response
} tf_msgtype_e;

struct tf_ctx_s {
  uint8_t   uuid[TF_UUID_LEN];  // UUID used in requests
  int       sk;        // socket to communicate with Transformer
  size_t    msg_bytes; // actual number of bytes in msg_buffer while filling it
                       // with request data or after receiving a response
  size_t    msg_idx;   // points to location in msg_buffer where to continue
                       // parsing the response
  tf_resp_t resp;      // one decoded response; a pointer to this is given to the caller
  bool      tmp_byte_set; // flag to indicate whether tmp_byte has a value
  uint8_t   tmp_byte;  // temporary storage for a byte so we can write '\0' in the
                       // msg buffer to terminate strings
  uint8_t   msg_buffer[TF_MAX_MESSAGE_SIZE + 1]; // +1 so we have room to write a '\0'
                                                 // so we can return pointers to strings
                                                 // instead of having to copy
};

static int connect_to_transformer(void)
{
  // just to be sure create the socket with the close-on-exec flag set
  int sk = socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);
  if (sk >= 0)
  {
    struct sockaddr_un server_address;
    int on = 1;
    struct timeval rcvtimeout = { .tv_sec = TF_RECEIVE_TIMEOUT };

    // Set socket option to include credentials in every call to the server
    setsockopt(sk, SOL_SOCKET, SO_PASSCRED, &on, sizeof(on));

    // Set receive timeout so we don't hang indefinitely in case
    // Transformer never sends back a reply.
    setsockopt(sk, SOL_SOCKET, SO_RCVTIMEO, &rcvtimeout, sizeof(rcvtimeout));

    // Connect to Transformer.
    memset(&server_address, 0, sizeof(struct sockaddr_un));
    server_address.sun_family = AF_UNIX;
    memcpy(&server_address.sun_path[1], TF_TRANSFORMER_ADDRESS, sizeof(TF_TRANSFORMER_ADDRESS) - 1);
    if (connect(sk, (struct sockaddr *) &server_address, TF_ABSTRACT_SUN_LEN) < 0)
    {
      TF_LOG_CRIT("connect() failed: %s", strerror(errno));
      close(sk);
      return -1;
    }
  }
  else
  {
    TF_LOG_CRIT("socket() failed: %s", strerror(errno));
  }
  TF_LOG_DBG("sk=%d", sk);
  return sk;
}

uint32_t tf_get_version(void)
{
  return LIBTRANSFORMER_VERSION;
}

tf_ctx_t* tf_new_ctx(const uint8_t uuid[TF_UUID_LEN], size_t uuid_len)
{
  TF_LOG_DBG("uuid=%p, uuid_len=%zu", uuid, uuid_len);
  // sanity check on provided UUID
  if (uuid && (uuid_len != TF_UUID_LEN))
  {
    TF_LOG_CRIT("bad UUID");
    return NULL;
  }
  // allocate ctx
  tf_ctx_t* ctx = calloc(1, sizeof(tf_ctx_t));
  if (!ctx)
  {
    TF_LOG_CRIT("failed to allocate ctx");
    return NULL;
  }
  // if a UUID was provided then use it
  if (uuid)
  {
    memcpy(ctx->uuid, uuid, TF_UUID_LEN);
  }
  else // otherwise generate a UUID ourselves
  {
    int fd = open("/dev/urandom", O_RDONLY | O_CLOEXEC);
    if (fd == -1)
    {
      TF_LOG_CRIT("failed to open %s: %s", "/dev/urandom", strerror(errno));
      free(ctx);
      return NULL;
    }
    ssize_t ret = read(fd, ctx->uuid, sizeof(ctx->uuid));
    if (ret != sizeof(ctx->uuid))
    {
      TF_LOG_CRIT("failed to read enough bytes from %s: %s", "/dev/urandom",
                  (ret == -1) ? strerror(errno) : "(no errmsg)");
      close(fd);
      free(ctx);
      return NULL;
    }
    close(fd);
  }
  // initialize all fields
  tf_reset_request(ctx);
  // connect to Transformer
  ctx->sk = connect_to_transformer();
  if (ctx->sk == -1)
  {
    free(ctx);
    return NULL;
  }
  TF_LOG_DBG("new ctx=%p", ctx);
  return ctx;
}

void tf_free_ctx(tf_ctx_t* ctx)
{
  TF_LOG_DBG("ctx=%p", ctx);
  if (ctx)
  {
    if (ctx->sk != -1)
    {
      tf_reset_request(ctx);
      close(ctx->sk);
    }
    free(ctx);
  }
}

/*
 * Encode a number in the serialization buffer.
 * Returns true if successful and false otherwise.
 * If the encoding failed some data might have been
 * copied to the serialization buffer.
 */
static bool encode_number(tf_ctx_t* ctx, uint16_t number)
{
  if (ctx->msg_bytes + sizeof(number) >= sizeof(ctx->msg_buffer))
  {
    // msg buffer would be full
    return false;
  }
  number = htons(number);
  memcpy(&ctx->msg_buffer[ctx->msg_bytes], &number, sizeof(number));
  ctx->msg_bytes += sizeof(number);
  return true;
}

/*
 * Encode a string in the serialization buffer.
 * Returns true if successful and false otherwise.
 * If the encoding failed some data might have been
 * copied to the serialization buffer.
 */
static bool encode_string(tf_ctx_t* ctx, const char* s)
{
  size_t len = strlen(s);

  if (len > UINT16_MAX)
  {
    return false;
  }
  uint16_t s_len = len;
  if (!encode_number(ctx, s_len))
  {
    return false;
  }
  if ((ctx->msg_bytes + s_len) >= sizeof(ctx->msg_buffer))
  {
    // msg buffer would be full
    return false;
  }
  memcpy(&ctx->msg_buffer[ctx->msg_bytes], s, s_len);
  ctx->msg_bytes += s_len;
  return true;
}

/*
 * Check if the serialization buffer is already in use. If so, the
 * contents must be for the same type of message. Otherwise the
 * pending request is reset.
 * If the 'check_single_use' flag is set then the buffer can only be
 * used for one request item.
 */
static bool check_msg_buffer(tf_ctx_t* ctx, tf_msgtype_e msgtype, bool check_single_use)
{
  if (ctx->msg_buffer[0] != MSG_UNKNOWN && ctx->msg_buffer[0] != msgtype)
  {
    TF_LOG_DBG("a previous request %d is still pending; resetting to %d", ctx->msg_buffer[0], msgtype);
    tf_reset_request(ctx);
  }
  if (check_single_use && ctx->msg_buffer[0] == msgtype)
  {
    return false;
  }
  ctx->msg_buffer[0] = msgtype;
  return true;
}

tf_err_e tf_fill_request(tf_ctx_t* ctx, const tf_req_t* req)
{
  if (!ctx || !req)
  {
    return TF_ERR_INVALID_ARG;
  }
  switch(req->type)
  {
    case TF_REQ_GPV:
      if (!req->u.gpv.path)
      {
        TF_LOG_ERR("no path provided");
        return TF_ERR_INVALID_ARG;
      }
      check_msg_buffer(ctx, MSG_GPV_REQ, false);
      TF_LOG_DBG("GPV: %s", req->u.gpv.path);
      if (!encode_string(ctx, req->u.gpv.path))
      {
        // TODO: reset request? if not, we should keep track of the
        // last index that still pointed to a valid msg (i.e. don't
        // leave the buffer with a half encoded request)
        return TF_ERR_RES_EXCEEDED;
      }
      break;
    case TF_REQ_SPV:
      if (!req->u.spv.full_path || !req->u.spv.value)
      {
        TF_LOG_ERR("no path or value provided");
        return TF_ERR_INVALID_ARG;
      }
      check_msg_buffer(ctx, MSG_SPV_REQ, false);
      TF_LOG_DBG("SPV: %s=%s", req->u.spv.full_path, req->u.spv.value);
      if (!encode_string(ctx, req->u.spv.full_path) ||
          !encode_string(ctx, req->u.spv.value))
      {
        // TODO: reset request? (see above)
        return TF_ERR_RES_EXCEEDED;
      }
      break;
    case TF_REQ_APPLY:
      TF_LOG_DBG("APPLY");
      if (!check_msg_buffer(ctx, MSG_APPLY_REQ, true))
      {
        TF_LOG_ERR("only one %s request item is possible in a request", "TF_REQ_APPLY");
        return TF_ERR_INVALID_ARG;
      }
      break;
    case TF_REQ_GPC:
      if (!req->u.gpc.path)
      {
        TF_LOG_ERR("no path provided");
        return TF_ERR_INVALID_ARG;
      }
      check_msg_buffer(ctx, MSG_GPC_REQ, false);
      TF_LOG_DBG("GPC: %s", req->u.gpc.path);
      if (!encode_string(ctx, req->u.gpc.path))
      {
        // TODO: reset request? (see above)
        return TF_ERR_RES_EXCEEDED;
      }
      break;
    case TF_REQ_ADD:
      if (!req->u.add.path)
      {
        TF_LOG_ERR("no path provided");
        return TF_ERR_INVALID_ARG;
      }
      if (!check_msg_buffer(ctx, MSG_ADD_REQ, true))
      {
        TF_LOG_ERR("only one %s request item is possible in a request", "TF_REQ_ADD");
        return TF_ERR_INVALID_ARG;
      }
      TF_LOG_DBG("ADD: %s%s", req->u.add.path, req->u.add.name ? req->u.add.name : "");
      if (!encode_string(ctx, req->u.add.path) ||
          (req->u.add.name && !encode_string(ctx, req->u.add.name)))
      {
        // TODO: reset request? (see above)
        return TF_ERR_RES_EXCEEDED;
      }
      break;
    case TF_REQ_DEL:
      if (!req->u.del.path)
      {
        TF_LOG_ERR("no path provided");
        return TF_ERR_INVALID_ARG;
      }
      if (!check_msg_buffer(ctx, MSG_DEL_REQ, true))
      {
        TF_LOG_ERR("only one %s request item is possible in a request", "TF_REQ_DEL");
        return TF_ERR_INVALID_ARG;
      }
      TF_LOG_DBG("DEL: %s", req->u.del.path);
      if (!encode_string(ctx, req->u.del.path))
      {
        // TODO: reset request? (see above)
        return TF_ERR_RES_EXCEEDED;
      }
      break;
    default:
      TF_LOG_ERR("invalid request type %d", req->type);
      return TF_ERR_INVALID_ARG;
  }
  return TF_ERR_OK;
}

static bool do_receive(tf_ctx_t* ctx)
{
  ssize_t ret;

restart:
  TF_LOG_DBG("sk = %d", ctx->sk);
  ret = read(ctx->sk, ctx->msg_buffer, sizeof(ctx->msg_buffer));
  if (ret > 0)
  {
    TF_LOG_DBG("received %zd bytes", ret);
    ctx->msg_bytes = ret;
    ctx->msg_idx = 1;
    ctx->tmp_byte_set = false;
    return true;
  }
  else if (ret < 0)
  {
    // Even though our callers probably register their signal handlers with
    // the SA_RESTART flag this does not restart recv()/read() when the socket
    // has a timeout set (see signal(7)) so we have to explicitly handle the
    // EINTR case. An interrupted recv()/read() is not an error.
    if (errno == EINTR)
    {
      goto restart;
    }
    if (errno == EAGAIN || errno == EWOULDBLOCK)
    {
      TF_LOG_ERR("timeout %d reached", TF_RECEIVE_TIMEOUT);
    }
    else
    {
      TF_LOG_ERR("error: %s", strerror(errno));
    }
  }
  TF_LOG_WARN("closing connection due to previous error");
  close(ctx->sk);
  ctx->sk = -1;
  return false;
}

void tf_reset_request(tf_ctx_t* ctx)
{
  if (!ctx)
  {
    return;
  }
  // if we are in the middle of processing responses then we need
  // to make sure we read all response messages
  if (ctx->resp.type != 0)
  {
    while (!(ctx->msg_buffer[0] & 0x80))
    {
      TF_LOG_DBG("discarding response");
      if (!do_receive(ctx))
      {
        break;
      }
    }
  }
  ctx->msg_buffer[0] = MSG_UNKNOWN;
  memcpy(&ctx->msg_buffer[1], ctx->uuid, sizeof(ctx->uuid));
  ctx->msg_bytes = 1 + sizeof(ctx->uuid);
  ctx->msg_idx = 0;
  ctx->tmp_byte_set = false;
  memset(&ctx->resp, 0, sizeof(ctx->resp));
}

static bool expect_response(tf_msgtype_e msgtype)
{
  if (msgtype == MSG_APPLY_REQ)
  {
    return false;
  }
  return true;
}

static bool decode_number(tf_ctx_t* ctx, uint16_t* number)
{
  if (ctx->msg_idx + sizeof(*number) > ctx->msg_bytes)
  { // TODO: put this check in a macro
    TF_LOG_ERR("trying to read beyond buffer: %zu > %zu", ctx->msg_idx + sizeof(*number), ctx->msg_bytes);
    return false;
  }
  uint8_t first_byte = ctx->msg_buffer[ctx->msg_idx];
  if (ctx->tmp_byte_set)
  {
    first_byte = ctx->tmp_byte;
    ctx->tmp_byte_set = false;
  }
  *number = (((uint16_t)first_byte) << 8) + ctx->msg_buffer[ctx->msg_idx + 1];
  ctx->msg_idx += 2;
  return true;
}

static bool decode_string(tf_ctx_t* ctx, const char** str)
{
  uint16_t s_len;

  if (!decode_number(ctx, &s_len))
  {
    return false;
  }
  if ((ctx->msg_idx + s_len) > ctx->msg_bytes)
  {
    TF_LOG_ERR("trying to read beyond buffer: %zu > %zu", ctx->msg_idx + s_len, ctx->msg_bytes);
    return false;
  }
  *str = (char *)&ctx->msg_buffer[ctx->msg_idx];
  ctx->msg_idx += s_len;
  ctx->tmp_byte = ctx->msg_buffer[ctx->msg_idx];
  ctx->tmp_byte_set = true;
  ctx->msg_buffer[ctx->msg_idx] = '\0'; // nul-terminate the string
  return true;
}

static bool s_ptype2ptype(const char* s_ptype, tf_ptype_e* ptype)
{
  if (strcmp(s_ptype, "string") == 0)
  {
    *ptype = TF_PTYPE_STRING;
  }
  else if (strcmp(s_ptype, "boolean") == 0)
  {
    *ptype = TF_PTYPE_BOOLEAN;
  }
  else if (strcmp(s_ptype, "unsignedInt") == 0)
  {
    *ptype = TF_PTYPE_UINT;
  }
  else if (strcmp(s_ptype, "int") == 0)
  {
    *ptype = TF_PTYPE_INT;
  }
  else if (strcmp(s_ptype, "long") == 0)
  {
    *ptype = TF_PTYPE_LONG;
  }
  else if (strcmp(s_ptype, "unsignedLong") == 0)
  {
    *ptype = TF_PTYPE_ULONG;
  }
  else if (strcmp(s_ptype, "dateTime") == 0)
  {
    *ptype = TF_PTYPE_DATETIME;
  }
  else if (strcmp(s_ptype, "base64") == 0)
  {
    *ptype = TF_PTYPE_BASE64;
  }
  else if (strcmp(s_ptype, "hexBinary") == 0)
  {
    *ptype = TF_PTYPE_HEXBINARY;
  }
  else if (strcmp(s_ptype, "password") == 0)
  {
    *ptype = TF_PTYPE_PASSWORD;
  }
  else
  {
    // we can only get here if Transformer adds support for a new
    // paramtype but this function isn't updated
    TF_LOG_ERR("unknown paramtype %s", s_ptype);
    return false;
  }
  return true;
}

// decode the next response in the buffer; fetching more
// data from Transformer if needed
static bool decode_next_response(tf_ctx_t* ctx)
{
  // Are we at the end of the buffer?
  // It's possible there's only a tag byte in the buffer. In that case
  // we simply return an empty response.
  if ((ctx->msg_idx >= ctx->msg_bytes))
  {
    if (ctx->msg_bytes == 1)
    {
      ctx->resp.type = TF_RESP_EMPTY;
      return true;
    }
    // are we still expecting more responses?
    if (ctx->msg_buffer[0] & 0x80)
    {
      // no, so we're done
      return false;
    }

    uint8_t prev_resp_type = ctx->msg_buffer[0] & 0x7F;

    // receive next response
    if (!do_receive(ctx))
    {
      return false;
    }
    // sanity check: is the received response of the same type
    // as the previous one?
    if ((ctx->msg_buffer[0] & 0x7F) != prev_resp_type)
    {
      TF_LOG_ERR("unexpected response type %"PRIu8", expected %"PRIu8"\n",
                 ctx->msg_buffer[0] & 0x7F, prev_resp_type);
      return false;
    }
  }
  bool rc = false;
  switch(ctx->msg_buffer[0] & 0x7F)
  {
    case MSG_ERROR_RESP:
      ctx->resp.type = TF_RESP_ERROR;
      rc = decode_number(ctx, &ctx->resp.u.error.code) &&
           decode_string(ctx, &ctx->resp.u.error.msg);
      break;
    case MSG_GPV_RESP:
    {
      const char* s_ptype = NULL;
      ctx->resp.type = TF_RESP_GPV;
      rc = decode_string(ctx, &ctx->resp.u.gpv.partial_path) &&
           decode_string(ctx, &ctx->resp.u.gpv.param) &&
           decode_string(ctx, &ctx->resp.u.gpv.value) &&
           decode_string(ctx, &s_ptype) &&
           s_ptype2ptype(s_ptype, &ctx->resp.u.gpv.ptype);
      break;
    }
    case MSG_SPV_RESP:
      // A SPV response contains 0 or more error records; if there were 0
      // then this is already handled at the beginning of the function.
      // We only get here if there's at least one error record.
      ctx->resp.type = TF_RESP_SPV_ERROR;
      rc = decode_number(ctx, &ctx->resp.u.spv_error.code) &&
           decode_string(ctx, &ctx->resp.u.spv_error.full_path) &&
           decode_string(ctx, &ctx->resp.u.spv_error.msg);
      break;
    case MSG_GPC_RESP:
      ctx->resp.type = TF_RESP_GPC;
      rc = decode_number(ctx, &ctx->resp.u.gpc.count);
      break;
    case MSG_ADD_RESP:
      ctx->resp.type = TF_RESP_ADD;
      rc = decode_string(ctx, &ctx->resp.u.add.instance);
      break;
    default:
      TF_LOG_ERR("unknown response type %d", ctx->msg_buffer[0]);
      return false;
  }
  return rc;
}

static bool do_send(tf_ctx_t* ctx)
{
  if (ctx->sk == -1)
  {
    TF_LOG_DBG("reconnecting to Transformer before sending");
    ctx->sk = connect_to_transformer();
    if (ctx->sk == -1)
    {
      return false;
    }
  }
  ctx->msg_buffer[0] |= 0x80;
  if (write(ctx->sk, ctx->msg_buffer, ctx->msg_bytes) < 0)
  {
    TF_LOG_ERR("error: %s", strerror(errno));
    close(ctx->sk);
    TF_LOG_DBG("reconnecting to Transformer after first send attempt");
    ctx->sk = connect_to_transformer();
    if (ctx->sk == -1)
    {
      return false;
    }
    if (write(ctx->sk, ctx->msg_buffer, ctx->msg_bytes) < 0)
    {
      TF_LOG_ERR("error again: %s", strerror(errno));
      close(ctx->sk);
      ctx->sk = -1;
      return false;
    }
  }
  return true;
}

const tf_resp_t* tf_next_response(tf_ctx_t* ctx, bool stop)
{
  if (!ctx)
  {
    return NULL;
  }
  // do we have to send the request first or can we return the
  // next response from our message buffer?
  if (ctx->resp.type == 0)
  {
    // response struct is empty; need to send request first
    tf_msgtype_e msgtype = ctx->msg_buffer[0];
    TF_LOG_DBG("response struct is empty; need to send request of type %d first", msgtype);
    if (msgtype == MSG_UNKNOWN)
    {
      // nothing to send; buffer is still empty
      TF_LOG_WARN("no request");
      return NULL;
    }
    if (!do_send(ctx))
    {
      // sending failed; clean up
      tf_reset_request(ctx);
      return NULL;
    }
    // receive first response, except for those requests that
    // do not return a response
    if (expect_response(msgtype))
    {
      if (!do_receive(ctx))
      {
        // receive failed; clean up
        tf_reset_request(ctx);
        return NULL;
      }
    }
    else if (!stop)
    {
      ctx->resp.type = TF_RESP_EMPTY;
      return &ctx->resp;
    }
  }
  // return next response, if any; reading next message from socket if needed
  if (stop || (ctx->resp.type == TF_RESP_EMPTY) || !decode_next_response(ctx))
  {
    TF_LOG_DBG("we're done");
    // we're done; clear everything
    tf_reset_request(ctx);
    return NULL;
  }

  return &ctx->resp;
}
