#ifndef CODIUS_SANDBOX_H
#define CODIUS_SANDBOX_H

#include <vector>
#include <unistd.h>

class SandboxPrivate;

class Sandbox {
  public:
    Sandbox();
    int exec(char** argv);

    virtual long int handleSyscall(long int id) = 0;
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
