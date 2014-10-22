#ifndef CODIUS_SANDBOX_IPC_H
#define CODIUS_SANDBOX_IPC_H

#define IPC_PARENT_IDX 0
#define IPC_CHILD_IDX 1

#include <uv.h>
#include <memory>

class SandboxIPC;

typedef void (*SandboxIPCCallback)(SandboxIPC& ipc, void* user_data);

struct SandboxIPC {
  SandboxIPC(int _dupAs);
  ~SandboxIPC();

  using Ptr = std::unique_ptr<SandboxIPC>;

  bool dup();
  bool startPoll(uv_loop_t* loop);
  bool stopPoll();

  uv_poll_t poll;
  int parent;
  int child;
  int dupAs;
  virtual void onReadReady() = 0;

private:
  static void cb_forward (uv_poll_t* req, int status, int events);
};

struct CallbackIPC : public SandboxIPC {
  CallbackIPC (int dupAs);
  void onReadReady() override;
  void setCallback(SandboxIPCCallback cb, void* user_data);

  using Ptr = std::unique_ptr<CallbackIPC>;
private:
  SandboxIPCCallback m_cb;
  void* m_cb_data;
};

#endif // CODIUS_SANDBOX_IPC_H
