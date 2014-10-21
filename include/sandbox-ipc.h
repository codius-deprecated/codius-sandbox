#ifndef CODIUS_SANDBOX_IPC_H
#define CODIUS_SANDBOX_IPC_H

#define IPC_PARENT_IDX 0
#define IPC_CHILD_IDX 1

#include <uv.h>

#include <memory>
struct SandboxIPC {
  SandboxIPC(int _dupAs);
  ~SandboxIPC();

  bool dup();
  bool startPoll(uv_loop_t* loop, uv_poll_cb cb, void* user_data);
  bool stopPoll();

  uv_poll_t poll;
  int parent;
  int child;
  int dupAs;
};

#endif // CODIUS_SANDBOX_IPC_H
