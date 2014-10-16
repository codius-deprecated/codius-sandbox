#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <assert.h>

#include "codius-util.h"

char*
codius_request_to_string (codius_request_t* request)
{
  JsonNode* req;
  JsonNode* args;
  char *buf;
  unsigned int i;

  assert (request);
  assert (request->api_name);
  assert (request->method_name);

  req = json_mkobject();
  json_append_member (req, "api", json_mkstring (request->api_name));
  json_append_member (req, "method", json_mkstring (request->method_name));
  args = json_mkarray();

  for(i = 0;i < 4; i++) {
    json_append_element (args, json_mknumber (request->data[i]));
  }

  json_append_member (req, "arguments", args);

  buf = json_encode (req);
  json_delete (req);

  return buf;
}

static int
send_request(const int fd, codius_request_t* request)
{
  char* buf;
  codius_rpc_header_t rpc_header;
  int ret = 0;
  
  buf = codius_request_to_string (request);
  rpc_header.magic_bytes = CODIUS_MAGIC_BYTES;
  rpc_header.callback_id = 0;
  rpc_header.size = strlen (buf);
  
  if (-1==write(fd, &rpc_header, sizeof(rpc_header)) ||
      -1==write(fd, buf, rpc_header.size)) {
    perror("write()");
    printf("Error writing to fd %d\n", fd);
    ret = -1;
  }

  free (buf);
  return ret;
}

codius_request_t*
codius_read_request(int fd)
{
  ssize_t bytes_read;
  codius_rpc_header_t rpc_header;
  codius_request_t* request = NULL;
  char* buf;

  bytes_read = read(fd, &rpc_header, sizeof(rpc_header));

  if (bytes_read==-1 || rpc_header.magic_bytes!=CODIUS_MAGIC_BYTES) {
    printf("Error reading from fd %d\n", fd);
    return NULL;
  }
  
  if (rpc_header.size > CODIUS_MAX_RESPONSE_SIZE) {
    printf("Message too large from fd %d\n", fd);
    abort();
  }

  buf = malloc (rpc_header.size);
  bytes_read = read(fd, buf, rpc_header.size);

  if (bytes_read==-1) {
    perror("read()");
    printf("Error reading from fd %d\n", fd);
    goto out;
  }

  request = codius_request_from_string (buf);

out:
  free (buf);
  return request;
}

//TODO: Expose this with a codius_read_result()
static codius_result_t*
read_result(const int fd)
{
  ssize_t bytes_read;
  codius_rpc_header_t rpc_header;
  codius_result_t* result = NULL;
  char* buf;
  JsonNode* answer;

  bytes_read = read(fd, &rpc_header, sizeof(rpc_header));

  if (bytes_read==-1 || rpc_header.magic_bytes!=CODIUS_MAGIC_BYTES) {
    printf("Error reading from fd %d\n", fd);
    return NULL;
  }
  
  if (rpc_header.size > CODIUS_MAX_RESPONSE_SIZE) {
    printf("Message too large from fd %d\n", fd);
    abort();
  }

  buf = malloc (rpc_header.size);
  bytes_read = read(fd, buf, rpc_header.size);

  if (bytes_read==-1) {
    perror("read()");
    printf("Error reading from fd %d\n", fd);
    goto out;
  }

  answer = json_decode (buf);
  json_delete (answer);
  result = malloc (sizeof (*result));
  memset (result, 0, sizeof (*result));
  result->success = 1;

out:
  free (buf);
  return result;
}

/* Make synchronous function call outside the sandbox.
   Return valid JsonNode or NULL for error. */
codius_result_t*
codius_sync_call(codius_request_t* request)
{
  const int sync_fd = 3;

  send_request (sync_fd, request);
  return read_result (sync_fd);
}

void
codius_result_free (codius_result_t* result)
{
  if (result) {
    free (result->asStr);
    free (result);
  }
}

codius_request_t*
codius_request_new (const char* api_name, const char* method_name)
{
  codius_request_t* ret = malloc (sizeof (*ret));
  strcpy (ret->api_name, api_name);
  strcpy (ret->method_name, method_name);
  return ret;
}

void
codius_request_free (codius_request_t* request)
{
  free (request->api_name);
  free (request->method_name);
  free (request);
}

codius_request_t*
codius_request_from_string (const char* buf)
{
  JsonNode* req;
  req = json_decode (buf);
  json_delete (req);
  abort();
  return NULL;
}
