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

int
codius_write_request (const int fd, codius_request_t* request)
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

  request->_id = rpc_header.callback_id;
  request->_fd = fd;

out:
  free (buf);
  return request;
}

int
codius_write_result (int fd, codius_result_t* result)
{
  char* buf;
  codius_rpc_header_t rpc_header;
  int ret = 0;

  buf = codius_result_to_string (result);
  rpc_header.magic_bytes = CODIUS_MAGIC_BYTES;
  rpc_header.callback_id = result->_id;
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

char*
codius_result_to_string (codius_result_t* result)
{
  JsonNode* req;
  char* buf;

  assert (result);
  req = json_mkobject();
  json_append_member (req, "success", json_mknumber (result->success));
  if (result->asStr == NULL)
    json_append_member (req, "result", json_mknumber (result->success));
  else
    json_append_member (req, "result", json_mkstring (result->asStr));

  buf = json_encode (req);
  json_delete (req);
  return buf;
}

codius_result_t*
codius_read_result (const int fd)
{
  ssize_t bytes_read;
  codius_rpc_header_t rpc_header;
  codius_result_t* result = NULL;
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

  result = codius_result_from_string (buf);
  free (buf);

out:
  free (buf);
  return result;
}

/* Make synchronous function call outside the sandbox.
   Return valid JsonNode or NULL for error. */
codius_result_t*
codius_sync_call (codius_request_t* request)
{
  const int sync_fd = 3;

  codius_write_request (sync_fd, request);
  return codius_read_result (sync_fd);
}

codius_result_t*
codius_result_new ()
{
  codius_result_t* ret;
  ret = malloc (sizeof (*ret));
  memset (ret, 0, sizeof (*ret));
  return ret;
}

void
codius_result_free (codius_result_t* result)
{
  if (result) {
    free (result->asStr);
    free (result);
  }
}

static int next_request_id = 0;

codius_request_t*
codius_request_new (const char* api_name, const char* method_name)
{
  codius_request_t* ret = malloc (sizeof (*ret));
  strcpy (ret->api_name, api_name);
  strcpy (ret->method_name, method_name);
  //FIXME: gcc-4.8 lacks stdatomic.h, so we're stuck with gcc builtins :(
  //see also: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=58016
  ret->_id = __sync_fetch_and_add (&next_request_id, 1);
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
  codius_request_t* ret;
  JsonNode* req;
  JsonNode* child;
  char* api_name;
  char* method_name;

  req = json_decode (buf);

  child = json_find_member (req, "api_name");
  api_name = strdup (child->string_);
  child = json_find_member (req, "method_name");
  method_name = strdup (child->string_);
  child = json_find_member (req, "data");

  ret = codius_request_new (api_name, method_name);

  int i = 0;
  JsonNode* node;
  json_foreach (node, child) {
    ret->data[i] = node->number_;
    i++;
  }
  node = NULL;
  child = NULL;

  json_delete (req);

  free (api_name);
  free (method_name);

  return ret;
}

codius_result_t* codius_result_from_string (const char* buf)
{
  codius_result_t* ret;
  JsonNode* req;
  JsonNode* child;

  req = json_decode (buf);
  ret = codius_result_new ();
  ret->success = json_find_member (req, "success")->number_;
  child = json_find_member (req, "result");
  if (child->tag == JSON_STRING)
    ret->asStr = child->string_;
  else
    ret->asInt = child->number_;

  json_delete (req);

  return ret;
}

int codius_send_reply (codius_request_t* request, codius_result_t* result)
{
  result->_id = request->_id;
  return codius_write_result (request->_fd, result);
}
