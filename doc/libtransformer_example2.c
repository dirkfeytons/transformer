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

#include <stdio.h>
#include <inttypes.h>
#include "libtransformer.h"

int main(void)
{
  const uint8_t s_uuid[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f};
  tf_ctx_t* ctx = tf_new_ctx(s_uuid, sizeof(s_uuid));
  tf_req_t req = {
    .type = TF_REQ_SPV,
    .u.spv = { .full_path = "InternetGatewayDevice.ManagementServer.Username",
               .value = "new_username" }
  };
  tf_fill_request(ctx, &req);
  req.u.spv.full_path = "InternetGatewayDevice.ManagementServer.Password";
  req.u.spv.value = "new_password";
  tf_fill_request(ctx, &req);
  const tf_resp_t* resp;
  while ((resp = tf_next_response(ctx, false)))
  {
    switch (resp->type)
    {
      case TF_RESP_EMPTY:
        puts("SPV succeeded");
        break;
      case TF_RESP_SPV_ERROR:
        puts("** SPV error **");
        printf("%s: %s (%"PRIu16")\n", resp->u.spv_error.full_path,
               resp->u.spv_error.msg, resp->u.spv_error.code);
        break;
      default:
        break;
    }
  }
  req.type = TF_REQ_APPLY;
  tf_fill_request(ctx, &req);
  // just send the APPLY; I'm not interested in the result
  tf_next_response(ctx, true);
  tf_free_ctx(ctx);
  return 0;
}
