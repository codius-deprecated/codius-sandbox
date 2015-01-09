#ifndef CODIUS_SANDBOX_H
#define CODIUS_SANDBOX_H

#include "codius-util.h"

#include <uv.h>

#include <map>
#include <memory>
#include <vector>

class SandboxIPC;
class VFS;

/**
 * seccomp sandbox
 */
class Sandbox {
  public:
    Sandbox();
    virtual ~Sandbox();

    using Word = unsigned long;
    using Address = Word;

    /**
     * Represents a syscall
     */
    class SyscallCall {
      public:
        SyscallCall () : id(-1), pid(-1) {}
        SyscallCall (pid_t pid) : id(-1), pid(pid) {}

        /**
         * Syscall number
         */
        Word id;

        /**
         * Arguments to the syscall
         */
        Word args[6];

        Word returnVal;

        pid_t pid;
    };

    /**
     * Called when a syscall is called from within the sandbox. Return a
     * different SyscallCall object to rewrite the call used
     *
     * @param call Call that was attempted
     * @return Call that will be executed instead of @call
     */
    virtual SyscallCall handleSyscall(const SyscallCall &call);

    /**
     * Called when the sandboxed child receives a signal.
     *
     * @param signal Signal that the child recieved
     */
    virtual void handleSignal(int signal) = 0;
    
    /**
     * Called when the sandboxed child has exited
     * 
     * @param status Exit status of the child
     */
    virtual void handleExit(int status) = 0;

    virtual void handleExecEvent(pid_t pid);

    /**
     * Returns the child's PID
     */
    pid_t getChildPID() const;

    /**
     * Returns true after the child has called execve()
     */
    bool enteredMain() const;

    /**
     * Releases the sandbox's hold on the child. \b WARNING: Once this function
     * is called, the child process is only constrained by the limits put in
     * place prior to spawn()
     *
     * @param signal - The signal to send to the child upon release. Use 0 for
     * no signal.
     */
    void releaseChild(int signal);

    /**
     * Kills the child process with SIGKILL
     */
    void kill();
    
    VFS& getVFS() const;

  protected:
    void setupSandboxing();
    pid_t fork();
    void traceChild();
    void setEnteredMain(bool entered);
    void setScratchAddress(Address addr);

  private:

    pid_t m_pid;
    uv_signal_t m_signal;
    bool m_enteredMain;
    void handleSeccompEvent(pid_t pid);
    std::unique_ptr<VFS> m_vfs;

    static void handleTrap(uv_signal_t* handle, int signum);
};

#endif // CODIUS_SANDBOX_H
