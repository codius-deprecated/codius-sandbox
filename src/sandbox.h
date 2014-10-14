#ifndef CODIUS_SANDBOX_H
#define CODIUS_SANDBOX_H

#include <vector>
#include <unistd.h>

class SandboxPrivate;

class Sandbox {
  public:
    Sandbox();
    ~Sandbox();
    int exec(char** argv);

    typedef struct _SyscallCall {
      long int id;
      long int arg1;
      long int arg2;
      long int arg3;
      long int arg4;
      long int arg5;
      long int arg6;
    } SyscallCall;

    virtual SyscallCall handleSyscall(const SyscallCall &call) = 0;
    virtual void handleIPC(const std::vector<char> &request) = 0;
    virtual void handleSignal(int signal);

    pid_t getChildPID() const;
    void releaseChild(int signal);

  private:
    SandboxPrivate* m_p;
    int traceChild(int ipc_fds[2]);
    void execChild(char** argv, int ipc_fds[2]) __attribute__ ((noreturn));
    void handleSeccompEvent();
};

#endif // CODIUS_SANDBOX_H
