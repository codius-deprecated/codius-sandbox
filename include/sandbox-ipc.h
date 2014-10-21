#ifndef CODIUS_SANDBOX_IPC_H
#define CODIUS_SANDBOX_IPC_H

#define IPC_PARENT_IDX 0
#define IPC_CHILD_IDX 1

#include <uv.h>
#include <unistd.h>

#include <memory>
struct SandboxIPC {
  SandboxIPC(int _dupAs)
    : dupAs(_dupAs)
  {
    int ipc_fds[2];
    socketpair (AF_UNIX, SOCK_STREAM, 0, ipc_fds);
    child = ipc_fds[IPC_CHILD_IDX];
    parent = ipc_fds[IPC_PARENT_IDX];
  }

  ~SandboxIPC()
  {
    stopPoll();
  }

  bool dup()
  {
    if (dup2 (child, dupAs) != dupAs)
      return false;
    return true;
  }

  bool startPoll(uv_loop_t* loop, uv_poll_cb cb, void* user_data)
  {
    uv_poll_init_socket (loop, &poll, parent);
    poll.data = user_data;
    uv_poll_start (&poll, UV_READABLE, cb);
    return true; //FIXME: check errors
  }

  bool stopPoll()
  {
    uv_poll_stop (&poll);
    return true; //FIXME: check errors
  }

  uv_poll_t poll;
  int parent;
  int child;
  int dupAs;
};

#endif // CODIUS_SANDBOX_IPC_H
