#include "sandbox.h"

#include <errno.h>
#include <error.h>
#include <memory.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/ptrace.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <unistd.h>

#define ORIG_EAX 11
#define PTRACE_EVENT_SECCOMP 7

class SandboxPrivate {
  public:
    SandboxPrivate(Sandbox* d)
      : d (d) {}
    Sandbox* d;
    int ipcSocket;
    pid_t pid;
};

Sandbox::Sandbox()
  : m_p(new SandboxPrivate(this))
{
}

int Sandbox::exec(char **argv)
{
  SandboxPrivate *priv = m_p;
  int ipc_fds[2];

  socketpair (AF_UNIX, SOCK_STREAM, 0, ipc_fds);
  priv->pid = fork();

  if (priv->pid) {
    return traceChild(ipc_fds);
  } else {
    execChild(argv, ipc_fds);
  }
}

void
Sandbox::execChild(char** argv, int ipc_fds[2])
{
  close (ipc_fds[0]);
  dup2 (ipc_fds[1], 3);

  printf ("Launching %s\n", argv[0]);
  ptrace (PTRACE_TRACEME, 0, 0);
  raise (SIGSTOP);
  setenv ("LD_PRELOAD", "./out/Default/lib.target/libcodius-sandbox.so", 1);
  if (execvp (argv[0], &argv[0]) < 0) {
    error(EXIT_FAILURE, errno, "Could not start sandboxed module:");
  }
  __builtin_unreachable();
}

void
Sandbox::handleSyscall(long int id)
{
}

void
Sandbox::handleSeccompEvent()
{
  SandboxPrivate *priv = m_p;
  struct user_regs_struct regs;
  memset (&regs, 0, sizeof (regs));
  if (ptrace (PTRACE_GETREGS, priv->pid, 0, &regs) < 0) {
    error (EXIT_FAILURE, errno, "Failed to fetch registers");
  }
  handleSyscall(regs.orig_rax);
}

pid_t
Sandbox::getChildPID() const
{
  return m_p->pid;
}

void
Sandbox::handleSignal(int signal)
{}

void
Sandbox::releaseChild(int signal)
{
  ptrace (PTRACE_DETACH, m_p->pid, 0, signal);
}

int
Sandbox::traceChild(int ipc_fds[2])
{
  SandboxPrivate* priv = m_p;
  int status = 0;

  close (ipc_fds[1]);
  priv->ipcSocket = ipc_fds[0];
  ptrace (PTRACE_ATTACH, priv->pid, 0, 0);
  waitpid (priv->pid, &status, 0);
  ptrace (PTRACE_SETOPTIONS, priv->pid, 0,
      PTRACE_O_EXITKILL | PTRACE_O_TRACESECCOMP );
  ptrace (PTRACE_CONT, priv->pid, 0, 0);

  while (!WIFEXITED (status)) {
    sigset_t mask;
    fd_set read_set;
    struct timeval timeout;
    memset (&timeout, 0, sizeof (timeout));
    memset (&read_set, 0, sizeof (read_set));
    memset (&mask, 0, sizeof (mask));

    FD_ZERO (&read_set);
    FD_SET (priv->ipcSocket, &read_set);

    int ready_count = select (1, &read_set, 0, 0, &timeout);

    waitpid (priv->pid, &status, WNOHANG);

    if (ready_count > 0) {
      std::vector<char> buf;
      buf.resize(1024);
      read (priv->ipcSocket, buf.data(), buf.size());
      buf[sizeof (buf)] = 0;
      handleRPC(buf);
    } else if (WSTOPSIG (status) == SIGSEGV) {
    } else if (WSTOPSIG (status) == SIGTRAP && (status >> 8) == (SIGTRAP | PTRACE_EVENT_SECCOMP << 8)) {
      handleSeccompEvent();
    }
    ptrace (PTRACE_CONT, priv->pid, 0, 0);
  }
  printf ("Sandboxed module exited: %d\n", WEXITSTATUS (status));
  return WEXITSTATUS (status);
}
