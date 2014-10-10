#include "sandbox.h"

#include <unistd.h>
#include <map>
#include <string>
#include <memory>
#include <string.h>
#include <signal.h>
#include <asm/unistd.h>

namespace syscall_names {
#include "names.cpp"
}

class DummySandbox : public Sandbox {
  virtual long int handleSyscall(long int id) override {
    std::string syscallName = syscall_names::names[id];
    printf ("Sandboxed module ran syscall %s (%d)\n", syscallName.c_str(), id);
    if (id == __NR_write)
      return -1;
    return id;
  }

  virtual void handleIPC(const std::vector<char> &buf) override {
    printf ("Got IPC buf: %s\n", buf.data());
  }

  virtual void handleSignal(int signal) override {
    if (signal == SIGSEGV || signal == SIGABRT) {
      launchDebugger();
    }
  }

  void launchDebugger() __attribute__ ((noreturn)) {
    char pidstr[15];
    sprintf (pidstr, "%d", getChildPID());
    printf ("Launching debugger on PID %s\n", pidstr);
    releaseChild(SIGSTOP);
    _exit (execlp ("gdb", "gdb", "-p", pidstr, NULL));
    __builtin_unreachable();
  }
};

int main(int argc, char** argv)
{
  std::unique_ptr<DummySandbox> sandbox = std::unique_ptr<DummySandbox>(new DummySandbox());
  return sandbox->exec (&argv[1]);
}
