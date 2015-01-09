#include "exec-sandbox.h"

#include "process-reader.h"

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
  clearenv ();
  for (auto i = envp.cbegin(); i != envp.cend(); i++) {
    setenv (i->first.c_str(), i->second.c_str(), 1);
  }

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
  }
}
