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
#include <uv.h>
#include <memory>

#include <iostream>

#include "codius-util.h"

#define ORIG_EAX 11
#define PTRACE_EVENT_SECCOMP 7

struct SandboxWrap {
  SandboxPrivate* priv;
};

class SandboxPrivate {
  public:
    SandboxPrivate(Sandbox* d)
      : d (d),
        ipcSocket(0),
        pid(0) {}
    Sandbox* d;
    int ipcSocket;
    pid_t pid;
    uv_signal_t signal;
    uv_poll_t poll;
    void handleSeccompEvent();
};

Sandbox::Sandbox()
  : m_p(new SandboxPrivate(this))
{
}

void
Sandbox::kill()
{
  ::kill (m_p->pid, SIGKILL);
}

Sandbox::~Sandbox()
{
  kill();
  uv_signal_stop (&m_p->signal);
  uv_poll_stop (&m_p->poll);
  delete m_p;
}

void Sandbox::spawn(char **argv)
{
  SandboxPrivate *priv = m_p;
  int ipc_fds[2];

  socketpair (AF_UNIX, SOCK_STREAM, 0, ipc_fds);
  priv->pid = fork();

  if (priv->pid) {
    traceChild(ipc_fds);
  } else {
    execChild(argv, ipc_fds);
  }
}

void
Sandbox::execChild(char** argv, int ipc_fds[2])
{
  scmp_filter_ctx ctx;

  if (dup2 (ipc_fds[IPC_CHILD_IDX], 3) !=3) {
    error (EXIT_FAILURE, errno, "Could not bind IPC channel");
  };

  printf ("Launching %s\n", argv[0]);
  ptrace (PTRACE_TRACEME, 0, 0);
  raise (SIGSTOP);
  
  prctl (PR_SET_NO_NEW_PRIVS, 1);

  ctx = seccomp_init (SCMP_ACT_TRACE (0));
  seccomp_rule_add (ctx, SCMP_ACT_ALLOW, SCMP_SYS (write), 0);
  seccomp_rule_add (ctx, SCMP_ACT_ALLOW, SCMP_SYS (read), 0);
  seccomp_rule_add (ctx, SCMP_ACT_KILL, SCMP_SYS (ptrace), 0);

  if (0<seccomp_load (ctx))
    error(EXIT_FAILURE, errno, "Could not lock down sandbox");
  seccomp_release (ctx);

  char buf[2048];
  memset (buf, CODIUS_MAGIC_BYTES, sizeof (buf));
  clearenv ();
  setenv ("CODIUS_SCRATCH_BUFFER", buf, 1);

  if (execvp (argv[0], &argv[0]) < 0) {
    error(EXIT_FAILURE, errno, "Could not start sandboxed module");
  }
  __builtin_unreachable();
}

void
SandboxPrivate::handleSeccompEvent()
{
  struct user_regs_struct regs;
  memset (&regs, 0, sizeof (regs));
  if (ptrace (PTRACE_GETREGS, pid, 0, &regs) < 0) {
    error (EXIT_FAILURE, errno, "Failed to fetch registers");
  }

  Sandbox::SyscallCall call;

#ifdef __i386__
  call.id = regs.orig_eax;
  call.args[0] = regs.ebx;
  call.args[1] = regs.ecx;
  call.args[2] = regs.edx;
  call.args[3] = regs.esi;
  call.args[4] = regs.edi;
  call.args[5] = regs.ebp;
#else
  call.id = regs.orig_rax;
  call.args[0] = regs.rbx;
  call.args[1] = regs.rcx;
  call.args[2] = regs.rdx;
  call.args[3] = regs.rsi;
  call.args[4] = regs.rdi;
  call.args[5] = regs.rbp;
#endif

  call = d->handleSyscall (call);

#ifdef __i386__
  regs.orig_eax = call.id;
  regs.ebx = call.args[0];
  regs.ecx = call.args[1];
  regs.edx = call.args[2];
  regs.esi = call.args[3];
  regs.edi = call.args[4];
  regs.ebp = call.args[5];
#else
  regs.orig_rax = call.id;
  regs.rbx = call.args[0];
  regs.rcx = call.args[1];
  regs.rdx = call.args[2];
  regs.rsi = call.args[3];
  regs.rdi = call.args[4];
  regs.rbp = call.args[5];
#endif

  if (ptrace (PTRACE_SETREGS, pid, 0, &regs) < 0) {
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

static void
handle_trap(uv_signal_t *handle, int signum)
{
  SandboxWrap* wrap = static_cast<SandboxWrap*>(handle->data);
  SandboxPrivate* priv = wrap->priv;
  int status = 0;

  waitpid (priv->pid, &status, WNOHANG);

  if (WSTOPSIG (status) == SIGTRAP) {
    int s = status >> 8;
    if (s == (SIGTRAP | PTRACE_EVENT_SECCOMP << 8)) {
      priv->handleSeccompEvent();
    } else if (s == (SIGTRAP | PTRACE_EVENT_EXIT << 8)) {
      ptrace (PTRACE_GETEVENTMSG, priv->pid, 0, &status);
      uv_signal_stop (handle);
      priv->d->handleExit (WEXITSTATUS (status));
    }
  } else if (WSTOPSIG (status) > 0) {
    priv->d->handleSignal (WSTOPSIG (status));
  } else if (WIFEXITED (status)) {
    uv_signal_stop (handle);
    priv->d->handleExit (WEXITSTATUS (status));
  }
  ptrace (PTRACE_CONT, priv->pid, 0, 0);
}

static void
handle_ipc_read (uv_poll_t* req, int status, int events)
{
  std::vector<char> buf;
  SandboxWrap* wrap = static_cast<SandboxWrap*>(req->data);
  SandboxPrivate* priv = wrap->priv;
  codius_rpc_header_t header;
  memset (&header, 0, sizeof (header));

  if (read (priv->ipcSocket, &header, sizeof (header)) < 0)
    error(EXIT_FAILURE, errno, "couldnt read IPC header");
  if (header.magic_bytes != CODIUS_MAGIC_BYTES)
    error(EXIT_FAILURE, errno, "Got bad magic header via IPC");
  buf.resize (header.size);
  std::cout << "reading ipc data size " << buf.size() << std::endl;
  read (priv->ipcSocket, buf.data(), buf.size());
  buf[buf.size()] = 0;
  priv->d->handleIPC(buf);
  header.size = 1;
  write (priv->ipcSocket, &header, sizeof (header));
  write (priv->ipcSocket, "", 1);
}

void
Sandbox::traceChild(int ipc_fds[2])
{
  SandboxPrivate* priv = m_p;
  uv_loop_t* loop = uv_default_loop ();
  int status = 0;

  priv->ipcSocket = ipc_fds[IPC_PARENT_IDX];
  ptrace (PTRACE_ATTACH, priv->pid, 0, 0);
  waitpid (priv->pid, &status, 0);
  ptrace (PTRACE_SETOPTIONS, priv->pid, 0,
      PTRACE_O_EXITKILL | PTRACE_O_TRACESECCOMP);

  uv_signal_init (loop, &priv->signal);
  SandboxWrap* wrap = new SandboxWrap;
  wrap->priv = priv;
  priv->signal.data = wrap;
  uv_signal_start (&priv->signal, handle_trap, SIGCHLD);

  uv_poll_init_socket (loop, &priv->poll, priv->ipcSocket);
  priv->poll.data = wrap;
  uv_poll_start (&priv->poll, UV_READABLE, handle_ipc_read);

  ptrace (PTRACE_CONT, priv->pid, 0, 0);
}

void
Sandbox::handleExit(int status)
{}
