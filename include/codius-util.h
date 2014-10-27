#ifndef __CODIUS_UTIL_H_
#define __CODIUS_UTIL_H_

#include "json.h"

// 129 KB
#define CODIUS_MAX_MESSAGE_SIZE 132096
// 256 MB
#define CODIUS_MAX_RESPONSE_SIZE 268435456

#ifdef __cplusplus
extern "C" {
#endif

typedef struct codius_rpc_header_s codius_rpc_header_t;

#pragma pack(push)
#pragma pack(1)
struct codius_rpc_header_s {
  unsigned long magic_bytes;
  unsigned long callback_id;
  unsigned long size;
};
#pragma pack(pop)

typedef struct codius_result_s codius_result_t;

struct codius_result_s {
  int success;
  int asInt;
  char* asStr;
};

typedef struct codius_request_s codius_request_t;

struct codius_request_s {
  char* api_name;
  char* method_name;
  int data[4];
};

static const unsigned long CODIUS_MAGIC_BYTES = 0xC0D105FE;

/**
 * Sends a codius IPC request and blocks until a response is received
 *
 * @param request Request to send
 * @return The result of the request. Must be freed via codius_request_free()
 * @see codius_request_new()
 * @see codius_result_free()
 */
codius_result_t*
codius_sync_call(codius_request_t* request);

/**
 * Creates a new IPC request. Must be later freed with codius_request_free()
 * 
 * @param api_name API being requested
 * @param method_name Method on API to call
 * @return A new request
 * @see codius_request_free()
 */
codius_request_t* codius_request_new (const char* api_name,
                                      const char* method_name);

/**
 * Frees a previously created IPC request.
 *
 * @param request Request to free
 * @see codius_request_new
 */
void codius_request_free (codius_request_t* request);

/**
 * Reads an IPC request from a file descriptor
 *
 * @param fd File descriptor to read from
 * @return A new request. Must be freed with codius_request_free()
 * @see codius_write_request()
 * @see codius_request_from_string()
 */
codius_request_t* codius_read_request (int fd);

/**
 * Writes an IPC request to a file descriptor
 *
 * @param fd File descriptor to write to
 * @param request Request to send
 * @return 0 on success, non-zero on failure. Errno will also be set on failure.
 * @see codius_write_request()
 * @see codius_request_to_string()
 */
int codius_write_request (int fd, codius_request_t* request);

/**
 * Builds an IPC request from a json string
 *
 * @param buf String to read
 * @return The new request. Must be freed with codius_request_free()
 * @see codius_read_request()
 * @see codius_request_to_string
 */
codius_request_t* codius_request_from_string (const char* buf);

/**
 * Builds the json representation of an IPC request
 *
 * @param request Request to use
 * @return A new string. Must be freed when finished.
 * @see codius_request_from_string()
 * @see codius_write_request()
 */
char* codius_request_to_string (codius_request_t* request);

/**
 * Allocates a new IPC result
 *
 * @return Newly allocated IPC result. Must be freed with codius_result_free()
 * @see codius_result_free()
 */
codius_result_t* codius_result_new ();

/**
 * Frees a previously allocated IPC result
 *
 * @param result The result to free
 * @see codius_result_new
 */
void codius_result_free (codius_result_t* result);

/**
 * Reads an IPC result from a file descriptor
 *
 * @param fd File descriptor to read from
 * @return A new IPC result. Must be freed with codius_result_free()
 * @see codius_sync_call()
 * @see codius_write_result()
 */
codius_result_t* codius_read_result (int fd);

/**
 * Creates a new IPC result from a json string
 * 
 * @param buf String to read
 * @return A newly allocated IPC result. Must be freed with codius_result_free()
 */
codius_result_t* codius_result_from_string (const char* buf);

/**
 * Writes an IPC result to a file descriptor
 *
 * @param fd File descriptor to write to
 * @param result Result to write to @p fd
 * @return Zero on success, non-zero on failure. Errno will also be set on
 * failure.
 */
int codius_write_result (int fd, codius_result_t* result);

/**
 * Creates a JSON string from an IPC result
 *
 * @param result Result to use
 * @return A newly allocated string. Must be freed when finished with it.
 */
char* codius_result_to_string (codius_result_t* result);

#ifdef __cplusplus
}
#endif

#endif /* __CODIUS_UTIL_H_ */
