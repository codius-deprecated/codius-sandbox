#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>

#include "codius-util.h"

/* Make synchronous function call outside the sandbox.
   Return response_len or -1 for error. */
int codius_sync_call(const char* request_buf, size_t request_len,
                     char **response_buf, size_t *response_len) {
  size_t bytes_read;
  const int sync_fd = 3;
  int resp_len;

  codius_rpc_header_t rpc_header;
  rpc_header.magic_bytes = CODIUS_MAGIC_BYTES;
  rpc_header.callback_id = 0;
  rpc_header.size = request_len;
  
  if (-1==write(sync_fd, &rpc_header, sizeof(rpc_header)) ||
      -1==write(sync_fd, request_buf, request_len)) {
    perror("write()");
    printf("Error writing to fd %d\n", sync_fd);
    return -1;
  }

  raise (SIGUSR1);
  
  bytes_read = read(sync_fd, &rpc_header, sizeof(rpc_header));
  if (bytes_read==-1 || rpc_header.magic_bytes!=CODIUS_MAGIC_BYTES) {
    printf("Error reading from fd %d\n", sync_fd);
    return -1;
  }
  
  if (rpc_header.size > CODIUS_MAX_RESPONSE_SIZE) {
    printf("Message too large from fd %d\n", sync_fd);
    abort();
  }
  *response_len = rpc_header.size;  
  
  *response_buf = (char*) malloc(*response_len);
  
  bytes_read = read(sync_fd, *response_buf, *response_len);
  if (bytes_read==-1) {
    perror("read()");
    printf("Error reading from fd %d\n", sync_fd);
    fflush(stdout);

    return -1;
  }

  return *response_len;
}
