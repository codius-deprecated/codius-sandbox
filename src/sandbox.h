#ifndef CODIUS_SANDBOX_H
#define CODIUS_SANDBOX_H

#include <vector>
#include <unistd.h>

#define IPC_PARENT_IDX 0
#define IPC_CHILD_IDX 1

class SandboxPrivate;

class Sandbox {
  public:
    Sandbox();
    ~Sandbox();
    void spawn(char** argv);

    typedef struct _SyscallCall {
      long int id;
      long int args[6];
    } SyscallCall;

    virtual SyscallCall handleSyscall(const SyscallCall &call) = 0;
    virtual void handleIPC(const std::vector<char> &request) = 0;
    virtual void handleSignal(int signal);
    virtual void handleExit(int status);

    pid_t getChildPID() const;
    void releaseChild(int signal);
    void kill();

  private:
    SandboxPrivate* m_p;
    void traceChild(int ipc_fds[2]);
    void execChild(char** argv, int ipc_fds[2]) __attribute__ ((noreturn));
};

#endif // CODIUS_SANDBOX_H
