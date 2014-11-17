#include "exec-sandbox.h"

#include <cassert>

#include <error.h>
#include <memory.h>
#include <sys/user.h>
#include <sys/ptrace.h>
#include <unistd.h>

void
ExecSandbox::spawn(char** argv, std::map<std::string, std::string>& envp)
{
  if (fork()) {
    traceChild();
  } else {
    execChild(argv, envp);
  }
}

void
ExecSandbox::execChild(char** argv, std::map<std::string, std::string>& envp)
{
  char buf[2048];
  memset (buf, CODIUS_MAGIC_BYTES, sizeof (buf));
  clearenv ();
  for (auto i = envp.cbegin(); i != envp.cend(); i++) {
    setenv (i->first.c_str(), i->second.c_str(), 1);
  }
  setenv ("CODIUS_SCRATCH_BUFFER", buf, 1);

  if (execvp (argv[0], &argv[0]) < 0) {
    error(EXIT_FAILURE, errno, "Could not start sandboxed module");
  }
  __builtin_unreachable();
}

void
ExecSandbox::handleExecEvent(pid_t pid)
{
  if (!enteredMain()) {
    setEnteredMain (true);
    findScratchBuffer (pid);
  }
}

void
ExecSandbox::findScratchBuffer(pid_t pid)
{
  struct user_regs_struct regs;
  Sandbox::Address stackAddr;
  Sandbox::Address environAddr;
  Sandbox::Address strAddr;
  Sandbox::Address scratchAddr = 0;
  int argc;

  memset (&regs, 0, sizeof (regs));

  if (ptrace (PTRACE_GETREGS, pid, 0, &regs) < 0) {
    error (EXIT_FAILURE, errno, "Failed to fetch registers on exec");
  }

  stackAddr = regs.rsp;
  argc = peekData (pid, stackAddr);
  environAddr = stackAddr + (sizeof (stackAddr) * (argc+2));

  strAddr = peekData (pid, environAddr);
  while (strAddr != 0) {
    std::vector<char> buf (1024);
    std::string needle("CODIUS_SCRATCH_BUFFER=");
    readString (pid, strAddr, buf);
    environAddr += sizeof (stackAddr);
    if (strncmp (buf.data(), needle.c_str(), needle.length()) == 0) {
      scratchAddr = strAddr + needle.length();
      break;
    }
    strAddr = peekData (pid, environAddr);
  }
  assert (scratchAddr);
  setScratchAddress (scratchAddr);
}

