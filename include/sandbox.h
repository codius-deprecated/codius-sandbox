#ifndef CODIUS_SANDBOX_H
#define CODIUS_SANDBOX_H

#include <map>
#include <vector>
#include <unistd.h>
#include <memory>
#include <string>
#include "codius-util.h"

class SandboxPrivate;
class SandboxIPC;
class VFS;

//FIXME: This shouldn't be public API. It is only used for libuv
struct SandboxWrap {
  SandboxPrivate* priv;
};

/**
 * seccomp sandbox
 */
class Sandbox {
  public:
    Sandbox();
    ~Sandbox();

    /**
     * Spawns a binary inside this sandbox. Arguments are the same as for
     * execv(3)
     */
    void spawn(char** argv, std::map<std::string, std::string>& envp);

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
    virtual SyscallCall handleSyscall(const SyscallCall &call) = 0;

    /**
     * Called when an IPC request from within the sandbox is generated.
     *
     * @param IPC request
     * @return Result of handling the IPC request
     */
    virtual void handleIPC(codius_request_t*) = 0;

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

    /**
     * Map a sandbox-side file descriptor to an outside handler
     * 
     * @param ipc Pointer to a SandboxIPC object
     */
    void addIPC(std::unique_ptr<SandboxIPC>&& ipc);

    /**
     * Read a word of the child process' memory
     *
     * @param addr Address to read
     * @return Word at @p addr in child's memory
     */
    Word peekData (pid_t pid, Address addr);

    /**
     * Read a contiguous chunk of the child process' memory
     *
     * @param addr Address to start reading from
     * @length Bytes to read
     * @buf Buffer to write to
     * @return @p true if successful, @p false otherwise. @P errno will be set
     * upon failure.
     */
    bool copyData (pid_t pid, Address addr, size_t length, void* buf);

    /**
     * Read a contiguous chunk of the child process' memory, stopping at the
     * first null byte.
     *
     * @param addr Address to write to
     * @param maxLength Maximum length of string to read
     * @param buf Buffer that the read string will be written to
     * @return @p true if successful, @p false otherwise. @p errno will be set
     * upon failure.
     */
    bool copyString (pid_t pid, Address addr, size_t maxLength, char* buf);

    /**
     * Write a single word to the child process' memory
     * 
     * @param addr Address to write to
     * @param word Word to write
     * @return @p true if successful, @p false otherwise. @p errno will be set
     * on failure.
     */
    bool pokeData (pid_t pid, Address addr, Word word);

    /**
     * Write a chunk of data to the scratch buffer inside the child's memory,
     * and return the address it was written to
     *
     * @param length Length of @p buf
     * @param buf Data to write
     * @return Address the data was written to, aligned up to the nearest word.
     */
    Address writeScratch(size_t length, const char* buf);

    /**
     * Frees all used scratch memory for re-use
     */
    void resetScratch();

    /**
     * Writes a chunk of data to the child process' memory
     *
     * @param addr Address to write to
     * @param length Length of @p buf
     * @param buf Data to write
     * @return @p true if successful, @p false otherwise. @p errno will be set upon
     * error.
     */
    bool writeData (pid_t pid, Address addr, size_t length, const char* buf);

    /**
     * Returns the address of the scratch buffer inside the child process
     */
    Address getScratchAddress () const;

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


  private:
    SandboxPrivate* m_p;
    void traceChild();
    void execChild(char** argv, std::map<std::string, std::string>& envp) __attribute__ ((noreturn));
};

#endif // CODIUS_SANDBOX_H
