#ifndef __CODIUS_UTIL_H_
#define __CODIUS_UTIL_H_

#include "jsmn.h"

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


static const unsigned long CODIUS_MAGIC_BYTES = 0xC0D105FE;

int codius_sync_call(const char* request_buf, size_t request_len,
                     char **response_buf, size_t *response_len);

/**
 * Get the type of the result field.
 */
jsmntype_t codius_parse_json_type(char *js, size_t len, const char *field_name);

/**
 * Get the integer that is present from the result field.
 */
int codius_parse_json_int(char *js, size_t len, const char *field_name);

/**
 * Get the string that is present in the result field.
 */
int codius_parse_json_str(char *js, size_t len, const char *field_name, char *buf, size_t buf_size);

#ifdef __cplusplus
}
#endif

#endif /* __CODIUS_UTIL_H_ */
