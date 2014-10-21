#ifndef CODIUS_SANDBOX_IPC_H
#define CODIUS_SANDBOX_IPC_H

#define IPC_PARENT_IDX 0
#define IPC_CHILD_IDX 1

#include <uv.h>

#include <memory>
struct SandboxIPC {
  SandboxIPC(int _dupAs);
  ~SandboxIPC();

  void setCallback(uv_poll_cb cb, void* user_data);
  bool dup();
  bool startPoll(uv_loop_t* loop);
  bool stopPoll();

  uv_poll_t poll;
  int parent;
  int child;
  int dupAs;

private:
  uv_poll_cb m_cb;
  void* m_cb_data;
};

#endif // CODIUS_SANDBOX_IPC_H
