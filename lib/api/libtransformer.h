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

/** \file
 * A library to communicate with Transformer.
 */

/** \mainpage
 *
 * \section intro Introduction
 * libtransformer gives you a simple API to easily communicate with Transformer.
 * The basics for using the API are as follows:
 * - Before you can talk to Transformer you must first create a context using
 *   tf_new_ctx().
 * - When you have a valid context you start preparing a request by filling it
 *   with request items using tf_fill_request().
 * - If you ever need to start over you use tf_reset_request().
 * - When the request is ready you use tf_next_response() to send it to
 *   Transformer and process the responses.
 * - After having received all the responses you can start preparing a new
 *   request or free your context using tf_free_ctx().
 *
 * \section examples Examples
 * Here are a few code examples that show how to use the API. Note that for
 * simplicity error handling is omitted.
 *
 * \subsection ex_complete Retrieving the complete IGD datamodel
 * This example shows how to send a simple request retrieving a lot of values.
 * \include libtransformer_example1.c
 *
 * \subsection ex_change_apply Making changes and applying them
 * When making datamodel changes these changes are validated and stored
 * persistently but not yet active. To accomplish that a separate 'apply'
 * request needs to be sent which will cause e.g. daemons to reread their
 * configuration.
 *
 * This example also shows the caller providing a UUID and how you could
 * deal with requests that don't return a response.
 * \include libtransformer_example2.c
 */

#ifndef LIBTRANSFORMER_H
#define LIBTRANSFORMER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif


/**
 * The version of libtransformer you're compiling against.
 */
#define LIBTRANSFORMER_VERSION 0x000002  // 0.0.2

/**
 * The length of a UUID in bytes.
 *
 * @see tf_new_ctx()
 */
#define TF_UUID_LEN 16


/**
 * Retrieve the version of libtransformer you're running against.
 */
uint32_t tf_get_version(void);

/**
 * Opaque context that represents your connection with Transformer.
 */
typedef struct tf_ctx_s tf_ctx_t;

/**
 * Creates a new context and connects to Transformer.
 *
 * Note that this implies that Transformer must be up and
 * running. If not the context creation will fail.
 *
 * @param uuid A UUID of suitable length (#TF_UUID_LEN) that identifies you
 *             towards Transformer. If no UUID is given (i.e. NULL) then one
 *             is generated for you.
 * @param uuid_len The length of the UUID you're passing in. If you're not
 *                 providing a UUID yourselves then this parameter is ignored.
 * @return A new context or NULL if something went wrong.
 */
tf_ctx_t* tf_new_ctx(const uint8_t uuid[TF_UUID_LEN], size_t uuid_len);

/**
 * Free the context.
 *
 * Any pending requests or responses will be properly cleaned up.
 * A context must not be used anymore after calling this function.
 *
 * @param ctx The context to free. Passing a NULL pointer is allowed.
 */
void tf_free_ctx(tf_ctx_t* ctx);

/**
 * The various types of request items.
 */
typedef enum {
  TF_REQ_GPV,   /**< GetParameterValues. Retrieve the values of one or more datamodel locations.
                     Possible responses (see ::tf_resp_e) are one or more `TF_RESP_GPV` responses
                     with each the value of one parameter or a `TF_RESP_ERROR` response if the request
                     could not be processed properly. **/
  TF_REQ_SPV,   /**< SetParameterValues. Set the values of the given datamodel parameters to the
                     given values. Possible responses (see ::tf_resp_e) are a `TF_RESP_EMPTY`
                     response if the request was processed successfully or one or more
                     `TF_RESP_SPV_ERROR` responses with details for each failed request item. **/
  TF_REQ_APPLY, /**< Apply. Apply all the changes that have been done in earlier requests. The only
                     possible response (see ::tf_resp_e) is a `TF_RESP_EMPTY` response. **/
  TF_REQ_GPC,   /**< GetCount. Get the number of values that would be returned if a GetParameterValues
                     request is done with the exact same datamodel locations. Possible responses (see
                     ::tf_resp_e) are a `TF_RESP_GPC` response with the total count of values or a
                     `TF_RESP_ERROR` response if the request could not be processed properly. **/
  TF_REQ_ADD,   /**< AddObject. Create a new instance of a certain object type at a specific datamodel
                     location. Possible responses (see ::tf_resp_e) are a `TF_RESP_ADD` response with
                     the instance number or name of the new instance or a `TF_RESP_ERROR` response if
                     the request could not be processed properly. **/
  TF_REQ_DEL    /**< DeleteObject. Remove the specified object. Possible responses (see ::tf_resp_e)
                     are a `TF_RESP_EMPTY` response if the delete was successful or a `TF_RESP_ERROR`
                     response if the request could not be processed properly. **/
} tf_req_e;

/**
 * Various return codes used by library functions.
 */
typedef enum {
  TF_ERR_OK,           ///< Everything went fine; no error occurred.
  TF_ERR_INVALID_ARG,  ///< An invalid argument was provided.
  TF_ERR_RES_EXCEEDED  ///< Resources exceeded.
} tf_err_e;

/**
 * GetParameterValues request item.
 *
 * Multiple items of this type can be added to a request.
 */
typedef struct {
  const char* path;  /**< A full or partial datamodel path from which you want to retrieve
                          values. Must not be NULL. */
} tf_req_gpv_t;

/**
 * SetParameterValues request item.
 *
 * All request items sent in one request are either all applied or not at all.
 * Multiple items of this type can be added to a request.
 */
typedef struct {
  const char* full_path;  /**< A full datamodel path identifying which parameter you want
                               to change. Must not be NULL. */
  const char* value;      ///< The new parameter value. Must not be NULL.
} tf_req_spv_t;

/**
 * GetParameterCount request item.
 *
 * Multiple items of this type can be added to a request.
 */
typedef struct {
  const char* path;  /**< A full or partial datamodel path from which you want to retrieve
                          the parameter count. Must not be NULL. */
} tf_req_gpc_t;

/**
 * AddObject request item.
 *
 * Only one AddObject request item can be added to a request.
 */
typedef struct {
  const char* path;  /**< A partial datamodel path ending in a multi-instance object type of which
                          you want to create a new instance. Must not be NULL. */
  const char* name;  /**< Optionally a name can be provided for the new instance.
                          This is only applicable for name-based object types. */
} tf_req_add_t;

/**
 * DeleteObject request item.
 *
 * Only one DeleteObject request item can be added to a request.
 */
typedef struct {
  const char* path;  /**< A partial datamodel path ending in an instance that you want to delete.
                          Must not be NULL. */
} tf_req_del_t;

/**
 * A request item.
 *
 * One or possibly more of these request items make up a request that can be sent
 * to Transformer for execution.
 * Ownership of the request item and any date in it remain with the caller.
 * The library will not modify it and everything can be safely destroyed after
 * the library function call returns.
 *
 * @see tf_fill_request()
 */
typedef struct {
  tf_req_e type;  ///< The type of request item.
  union {
    tf_req_gpv_t gpv;  ///< Request item details in case it's a GetParameterValues.
    tf_req_spv_t spv;  ///< Request item details in case it's a SetParameterValues.
    tf_req_gpc_t gpc;  ///< Request item details in case it's a GetCount.
    tf_req_add_t add;  ///< Request item details in case it's a AddObject.
    tf_req_del_t del;  ///< Request item details in case it's a DeleteObject.
  } u;
} tf_req_t;

/**
 * Add a new request item for sending to Transformer.
 *
 * Only request items of the same type can be sent together. If a new request item
 * is added to a set of items of a different type then those are discarded.
 *
 * @param ctx A valid context.
 * @param req A request item. Ownership lies fully with the caller.
 *
 * @return A result code.
 */
tf_err_e tf_fill_request(tf_ctx_t* ctx, const tf_req_t* req);

/**
 * Reset any pending request.
 *
 * Any pending responses are also discarded if needed.
 *
 * @param ctx A valid context.
 */
void tf_reset_request(tf_ctx_t* ctx);

/**
 * The various types of response items.
 */
typedef enum {
  TF_RESP_ERROR = 100, /**< A generic error response. This usually means something was found
                            to be wrong with the values provided in the request. **/
  TF_RESP_EMPTY,       /**< An empty response. This means the request was processed and there
                            is nothing to return to the caller. **/
  TF_RESP_GPV,         ///< Details of a GetParameterValues response.
  TF_RESP_SPV_ERROR,   ///< Details of a SetParameterValues error response.
  TF_RESP_GPC,         ///< Details of a GetCount response.
  TF_RESP_ADD,         ///< Details of an AddObject response.
} tf_resp_e;

/**
 * The possible types of a parameter value.
 */
typedef enum {
  TF_PTYPE_STRING,    ///< A string, possibly empty.
  TF_PTYPE_UINT,      ///< An unsigned 32bit integer.
  TF_PTYPE_INT,       ///< A signed 32bit integer.
  TF_PTYPE_BOOLEAN,   /**< A boolean. Possible values are the strings `"0"` and `"false"`
                           for the boolean value `false`, and `"1"` and `"true"` for
                           the boolean value `true`. **/
  TF_PTYPE_DATETIME,  ///< Combined date and time according to ISO 8601.
  TF_PTYPE_BASE64,    ///< Base64 encoded binary.
  TF_PTYPE_ULONG,     ///< An unsigned 64bit integer.
  TF_PTYPE_LONG,      ///< A signed 64bit integer.
  TF_PTYPE_HEXBINARY, ///< Hex encoded binary.
  TF_PTYPE_PASSWORD   ///< A password string.
} tf_ptype_e;

/**
 * Error response.
 */
typedef struct {
  uint16_t    code;  ///< Error code.
  const char* msg;   ///< Error description.
} tf_resp_error_t;

/**
 * GetParameterValues response.
 */
typedef struct {
  const char* partial_path;  ///< A partial path pointing to a specific datamodel object.
  const char* param;         ///< The parameter name whose value is given.
  const char* value;         ///< The value of the parameter.
  tf_ptype_e  ptype;         ///< The type of the parameter.
} tf_resp_gpv_t;

/**
 * SetParameterValues error response.
 *
 * Returned for each SPV request item that could not be executed.
 */
typedef struct {
  const char* full_path;  ///< Full path identifying which parameter of which object failed to be changed.
  uint16_t    code;       ///< Error code.
  const char* msg;        ///< Error description.
} tf_resp_spv_error_t;

/**
 * GetParameterCount response.
 *
 * One such item is returned for a request containing one or more
 * GetParameterCount request items.
 */
typedef struct {
  uint16_t count;  /**< The number of parameter values that probably would be returned if
                        a GetParameterValues request was done on the same datamodel paths. */
} tf_resp_gpc_t;

/**
 * AddObject response.
 *
 * One such item is returned for a AddObject request.
 */
typedef struct {
  const char* instance;  ///< The index number or name of the new instance.
} tf_resp_add_t;

/**
 * A response item.
 *
 * Ownership of the response item and any containing data lies with the
 * library. The caller of the library must not change it. Pointers to data
 * are only valid until the next response is requested or a new request is
 * started.
 */
typedef struct {
  tf_resp_e type;  ///< The type of the response item.
  union {
    tf_resp_error_t     error;  ///< Response item details in case it's an Error response.
    tf_resp_gpv_t       gpv;    ///< Response item details in case it's a GetParameterValues response.
    tf_resp_spv_error_t spv_error; ///< Response item details in case it's a SetParameterValues Error response.
    tf_resp_gpc_t       gpc;    ///< Response item details in case it's a GetParameterCount response.
    tf_resp_add_t       add;    ///< Response item details in case it's an AddObject response.
  } u;
} tf_resp_t;

/**
 * Get the next response.
 *
 * If needed the request prepared with tf_fill_request() is first sent to Transformer.
 *
 * @param ctx A valid context.
 * @param stop Indicate whether you are still interested in responses or not. If
 *             'true' is given then all further responses are discarded and
 *             the context is prepared for a new request. Note that if this flag
 *             is set to 'true' the first time you call this function after
 *             preparing a request, then the request will still be sent. This is
 *             useful if you want to send a request that you know will only return
 *             an empty response or when you're not interested in the responses.
 * @return The next response or NULL if there are no further responses or something
 *         went wrong. Note that when NULL is returned the request is cleared and
 *         you need to use tf_fill_request() again.
 */
const tf_resp_t* tf_next_response(tf_ctx_t* ctx, bool stop);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // LIBTRANSFORMER_H
