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

codius_result_t*
codius_sync_call(codius_request_t* request);

codius_request_t*
codius_read_request(int fd);

void
codius_write_result(int fd, codius_result_t*);

codius_request_t*
codius_request_from_string(const char* buf);

char*
codius_request_to_string(codius_request_t* request);

codius_request_t*
codius_request_new(const char* api_name, const char* method_name);

void
codius_request_free (codius_request_t* request);

codius_result_t*
codius_result_new ();

void
codius_result_free (codius_result_t* result);

#ifdef __cplusplus
}
#endif

#endif /* __CODIUS_UTIL_H_ */
