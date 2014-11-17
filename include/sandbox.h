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

    virtual void handleExecEvent(pid_t pid);

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
    bool copyData (pid_t pid, Address addr, std::vector<char>& buf);

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
    bool readString (pid_t pid, Address addr, std::vector<char>& buf);

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
    Address writeScratch(pid_t pid, std::vector<char>& buf);

    /**
     * Frees all used scratch memory for re-use
     */
    void resetScratch();

    template<typename Type>
    bool writeData (pid_t pid, Address addr, const std::vector<Type>& buf)
    {
      return writeData (pid, addr, buf, buf.size());
    }

    template<typename Type>
    bool writeData (pid_t pid, Address addr, const std::vector<Type>& buf, size_t maxLen)
    {
      return writeData (pid, addr, reinterpret_cast<const char*> (buf.data()), std::min (maxLen, buf.size() * sizeof (Type)));
    }

    bool writeData (pid_t pid, Address addr, const std::string& str, size_t maxLen)
    {
      return writeData (pid, addr, str.c_str(), std::min (maxLen, str.size() * sizeof (char)));
    }

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

  protected:
    void setupSandboxing();
    pid_t fork();
    void traceChild();
    void setEnteredMain(bool entered);
    void setScratchAddress(Address addr);

  private:

    /**
     * Writes a chunk of data to the child process' memory
     *
     * @param addr Address to write to
     * @param length Length of @p buf
     * @param buf Data to write
     * @return @p true if successful, @p false otherwise. @p errno will be set upon
     * error.
     */
    bool writeData (pid_t pid, Address addr, const char* buf, size_t length);

    std::vector<std::unique_ptr<SandboxIPC> > m_ipcSockets;
    pid_t m_pid;
    uv_signal_t m_signal;
    bool m_enteredMain;
    Sandbox::Address m_scratchAddr;
    Sandbox::Address m_nextScratchSegment;
    void handleSeccompEvent(pid_t pid);
    VFS* m_vfs;

    static void readIPC(SandboxIPC& ipc, void* data);
    static void handleTrap(uv_signal_t* handle, int signum);
};

#endif // CODIUS_SANDBOX_H
