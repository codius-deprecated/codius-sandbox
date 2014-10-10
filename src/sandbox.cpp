#include "sandbox.h"

#include <errno.h>
#include <error.h>
#include <memory.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/prctl.h>
#include <sys/ptrace.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <unistd.h>
#include <seccomp.h>
#include <sched.h>

#include "codius-util.h"

#define ORIG_EAX 11
#define PTRACE_EVENT_SECCOMP 7
#define IPC_PARENT_IDX 0
#define IPC_CHILD_IDX 1

class SandboxPrivate {
  public:
    SandboxPrivate(Sandbox* d)
      : d (d),
        ipcSocket(0),
        pid(0) {}
    Sandbox* d;
    int ipcSocket;
    pid_t pid;
};

Sandbox::Sandbox()
  : m_p(new SandboxPrivate(this))
{
}

Sandbox::~Sandbox()
{
  delete m_p;
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
  scmp_filter_ctx ctx;

  close (ipc_fds[IPC_PARENT_IDX]);
  if (dup2 (ipc_fds[IPC_CHILD_IDX], 3) !=3) {
    error (EXIT_FAILURE, errno, "Could not bind IPC channel");
  };

  printf ("Launching %s\n", argv[0]);
  ptrace (PTRACE_TRACEME, 0, 0);
  raise (SIGSTOP);
  
  prctl (PR_SET_NO_NEW_PRIVS, 1);

  ctx = seccomp_init (SCMP_ACT_TRACE (0));
  seccomp_rule_add (ctx, SCMP_ACT_KILL, SCMP_SYS (ptrace), 0);

  if (0<seccomp_load (ctx))
    error(EXIT_FAILURE, errno, "Could not lock down sandbox");
  seccomp_release (ctx);

  if (execvp (argv[0], &argv[0]) < 0) {
    error(EXIT_FAILURE, errno, "Could not start sandboxed module");
  }
  __builtin_unreachable();
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

#ifdef __i386__
  regs.orig_eax = handleSyscall(regs.orig_eax);
#else
  regs.orig_rax = handleSyscall(regs.orig_rax);
#endif

  if (ptrace (PTRACE_SETREGS, priv->pid, 0, &regs) < 0) {
    error (EXIT_FAILURE, errno, "Failed to set registers");
  }
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

  close (ipc_fds[IPC_CHILD_IDX]);
  priv->ipcSocket = ipc_fds[IPC_PARENT_IDX];
  ptrace (PTRACE_ATTACH, priv->pid, 0, 0);
  waitpid (priv->pid, &status, 0);
  ptrace (PTRACE_SETOPTIONS, priv->pid, 0,
      PTRACE_O_EXITKILL | PTRACE_O_TRACESECCOMP);
  ptrace (PTRACE_CONT, priv->pid, 0, 0);

  while (!WIFEXITED (status)) {
    sigset_t mask;
    fd_set read_set;
    struct timeval timeout;

    memset (&timeout, 0, sizeof (timeout));
    memset (&mask, 0, sizeof (mask));

    FD_ZERO (&read_set);
    FD_SET (priv->ipcSocket, &read_set);

    waitpid (priv->pid, &status, 0);

    if (WSTOPSIG (status) == SIGTRAP) {
      int s = status >> 8;
      if (s == (SIGTRAP | PTRACE_EVENT_SECCOMP << 8)) {
        handleSeccompEvent();
      } else if (s == (SIGTRAP | PTRACE_EVENT_EXIT << 8)) {
        ptrace (PTRACE_GETEVENTMSG, priv->pid, 0, &status);
        printf("exit\n");
        return status;
      }
    } else if (WSTOPSIG (status) == SIGUSR1) {
      codius_rpc_header_t header;
      std::vector<char> buf;

      if (read (priv->ipcSocket, &header, sizeof (header)) < 0)
        error(EXIT_FAILURE, errno, "couldnt read IPC header");
      if (header.magic_bytes != CODIUS_MAGIC_BYTES)
        error(EXIT_FAILURE, errno, "Got bad magic header via IPC");
      buf.resize (header.size);
      read (priv->ipcSocket, buf.data(), buf.size());
      buf[buf.size()] = 0;
      handleIPC(buf);
      write (priv->ipcSocket, &header, sizeof (header));
      write (priv->ipcSocket, "", 1);
    } else if (WSTOPSIG (status) > 0) {
      handleSignal (WSTOPSIG (status));
    }
    ptrace (PTRACE_CONT, priv->pid, 0, 0);
  }
  printf ("Sandboxed module exited: %d\n", WEXITSTATUS (status));
  return WEXITSTATUS (status);
}
