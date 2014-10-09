#ifndef CODIUS_SANDBOX_H
#define CODIUS_SANDBOX_H

class SandboxPrivate;

class Sandbox {
  public:
    Sandbox();
    int exec(char** argv);

  private:
    SandboxPrivate* m_p;
    int traceChild(int ipc_fds[2]);
    void execChild(char** argv, int ipc_fds[2]) __attribute__ ((noreturn));
    void launchDebugger() __attribute__ ((noreturn));
    void handleSeccompEvent();
};

#endif // CODIUS_SANDBOX_H
