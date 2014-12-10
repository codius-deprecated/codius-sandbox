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
CallbackIPC::setCallback(SandboxIPCCallback cb, void* user_data)
{
  m_cb = cb;
  m_cb_data = user_data;
}

CallbackIPC::~CallbackIPC()
{}

CallbackIPC::CallbackIPC(int dupAs)
  : SandboxIPC (dupAs)
{}

void
SandboxIPC::cb_forward(uv_poll_t* req, int status, int events)
{
  SandboxIPC* self = static_cast<SandboxIPC*>(req->data);
  self->onReadReady();
}

void
CallbackIPC::onReadReady()
{
  m_cb (*this, m_cb_data);
}

bool
SandboxIPC::startPoll(uv_loop_t* loop)
{
  uv_poll_init_socket (loop, &poll, parent);
  poll.data = this;
  if (uv_poll_start (&poll, UV_READABLE, SandboxIPC::cb_forward) < 0)
    return false;
  return true;
}

bool
SandboxIPC::stopPoll()
{
  if (uv_poll_stop (&poll) < 0)
    return false;
  return true;
}
