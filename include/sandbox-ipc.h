#ifndef CODIUS_SANDBOX_IPC_H
#define CODIUS_SANDBOX_IPC_H

#define IPC_PARENT_IDX 0
#define IPC_CHILD_IDX 1

#include <uv.h>
#include <memory>

class SandboxIPC;

typedef void (*SandboxIPCCallback)(SandboxIPC& ipc, void* user_data);

/**
 * Class that binds a file descriptor inside the sandbox with an external
 * handler
 */
class SandboxIPC {
public:
  /**
   * Constructor
   *
   * @param _dupAs File descriptor that will be exposed within the sandbox
   */
  SandboxIPC(int _dupAs);
  ~SandboxIPC();

  using Ptr = std::unique_ptr<SandboxIPC>;

  /**
   * calls dup2(child, dupAs), resulting in the descriptor referred to by @p dupAs
   * now pointing to @p child.
   */
  bool dup();

  /**
   * Attaches the parent side of the IPC channel to the libuv event loop
   *
   * @param loop A libuv event loop
   */
  bool startPoll(uv_loop_t* loop);

  /**
   * Removes the parent side of the IPC channel from the libuv event loop
   */
  bool stopPoll();

  uv_poll_t poll;

  /**
   * Parent side of the IPC pipe
   */
  int parent;

  /**
   * Child side of the IPC pipe
   */
  int child;

  /**
   * File descriptor that will be connected through the sandbox
   */
  int dupAs;

  /**
   * Called when there is data ready to be read out of the sandbox
   */
  virtual void onReadReady() = 0;

private:
  static void cb_forward (uv_poll_t* req, int status, int events);
};

/**
 * An implementation of @p SandboxIPC that executes a callback when data is
 * ready to be read out of the sandbox
 */
class CallbackIPC : public SandboxIPC {
public:
  CallbackIPC (int dupAs);
  void onReadReady() override;

  /**
   * Sets the callback that will be excuted when data is ready to be read out of
   * the sandbox
   *
   * @param cb Callback to run
   * @param user_data Data to pass to the callback
   */
  void setCallback(SandboxIPCCallback cb, void* user_data);

  using Ptr = std::unique_ptr<CallbackIPC>;
private:
  SandboxIPCCallback m_cb;
  void* m_cb_data;
};

#endif // CODIUS_SANDBOX_IPC_H
