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
  tf_ctx_t* ctx = tf_new_ctx(NULL, 0);
  tf_req_t req ={
    .type = TF_REQ_GPV,
    .u.gpv.path = "InternetGatewayDevice."
  };
  tf_fill_request(ctx, &req);
  const tf_resp_t* resp;
  while( (resp = tf_next_response(ctx, false)))
  {
    switch (resp->type)
    {
      case TF_RESP_GPV:
        printf("%s%s=%s (%d)\n", resp->u.gpv.partial_path,
               resp->u.gpv.param, resp->u.gpv.value, resp->u.gpv.ptype);
        break;
      case TF_RESP_ERROR:
        puts("** Error **");
        printf("%"PRIu16": %s\n", resp->u.error.code, resp->u.error.msg);
        break;
      default:
        break;
    }
  }
  tf_free_ctx(ctx);
  return 0;
}
