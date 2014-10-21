#include "sandbox-ipc.h"
#include <unistd.h>

SandboxIPC::SandboxIPC(int _dupAs)
  : dupAs (_dupAs)
{
  int ipc_fds[2];
  socketpair (AF_UNIX, SOCK_STREAM, 0, ipc_fds);
  child = ipc_fds[IPC_CHILD_IDX];
  parent = ipc_fds[IPC_PARENT_IDX];
}

SandboxIPC::~SandboxIPC()
{
  stopPoll();
  close (child);
  close (parent);
}

bool
SandboxIPC::dup()
{
  if (dup2 (child, dupAs) != dupAs)
    return false;
  return true;
}

void
SandboxIPC::setCallback(uv_poll_cb cb, void* user_data)
{
  m_cb = cb;
  m_cb_data = user_data;
}

bool
SandboxIPC::startPoll(uv_loop_t* loop)
{
  uv_poll_init_socket (loop, &poll, parent);
  poll.data = m_cb_data;
  uv_poll_start (&poll, UV_READABLE, m_cb);
  return true; //FIXME: check errors
}

bool
SandboxIPC::stopPoll()
{
  uv_poll_stop (&poll);
  return true; //FIXME: check errors
}
