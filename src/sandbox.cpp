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
        pid(0),
        entered_main(false),
        scratchAddr(0) {}
    Sandbox* d;
    int ipcSocket;
    pid_t pid;
    uv_signal_t signal;
    uv_poll_t poll;
    bool entered_main;
    long long scratchAddr;
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

long int
Sandbox::peekData(long long addr)
{
  return ptrace (PTRACE_PEEKDATA, m_p->pid, addr, NULL);
}

// FIXME: long long being copied into a void* blob?
// Triple check sizes and offsets!
int
Sandbox::copyData(unsigned long long addr, size_t length, void* buf)
{
  for (int i = 0; i < length; i++) {
    long long ret = peekData (addr+i);
    memcpy (buf+i, &ret, sizeof (ret));
  }
  if (errno)
    return -1;
  return 0;
}

int
Sandbox::copyString (long long addr, int maxLength, char* buf)
{
  for (int i = 0; i < maxLength; i++) {
    buf[i] = peekData(addr + i);
    if (buf[i] == 0)
      break;
  }

  if (errno)
    return -1;
  return 0;
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
  call.args[0] = regs.rdi;
  call.args[1] = regs.rsi;
  call.args[2] = regs.rdx;
  call.args[3] = regs.rcx;
  call.args[4] = regs.r8;
  call.args[5] = regs.r9;
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
  regs.rdi = call.args[0];
  regs.rsi = call.args[1];
  regs.rdx = call.args[2];
  regs.rcx = call.args[3];
  regs.r8 = call.args[4];
  regs.r9 = call.args[5];
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

long long
Sandbox::getScratchAddress() const
{
  return m_p->scratchAddr;
}

int
Sandbox::pokeData(long long addr, long long word)
{
  return ptrace (PTRACE_POKEDATA, m_p->pid, addr, word);
}

int
Sandbox::writeScratch(size_t length, const char* buf)
{
  return writeData (m_p->scratchAddr, length, buf);
}

int
Sandbox::writeData (unsigned long long addr, size_t length, const char* buf)
{
  for (int i = 0; i < length; i++) {
    pokeData (m_p->scratchAddr + i, buf[i]);
  }
  if (errno)
    return -1;
  return 0;
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
      uv_poll_stop (&priv->poll);
      priv->d->handleExit (WEXITSTATUS (status));
    } else if (s == (SIGTRAP | PTRACE_EVENT_EXEC << 8)) {
      if (!priv->entered_main) {
        struct user_regs_struct regs;
        long long stackAddr;
        long long environAddr;
        long long strAddr;
        int argc;

        priv->entered_main = true;

        memset (&regs, 0, sizeof (regs));

        if (ptrace (PTRACE_GETREGS, priv->pid, 0, &regs) < 0) {
          error (EXIT_FAILURE, errno, "Failed to fetch registers on exec");
        }

        stackAddr = regs.rsp;
        priv->d->copyData (stackAddr, sizeof (argc), &argc);
        environAddr = stackAddr + (sizeof (stackAddr) * (argc+2));

        strAddr = priv->d->peekData (environAddr);
        while (strAddr != 0) {
          char buf[1024];
          std::string needle("CODIUS_SCRATCH_BUFFER=");
          priv->d->copyString (strAddr, sizeof (buf), buf);
          environAddr += sizeof (stackAddr);
          if (strncmp (buf, needle.c_str(), needle.length()) == 0) {
            priv->scratchAddr = strAddr + needle.length();
            break;
          }
          strAddr = priv->d->peekData (environAddr);
        }
        assert (priv->scratchAddr);
      }
    } else {
      abort();
    }
  } else if (WSTOPSIG (status) > 0) {
    priv->d->handleSignal (WSTOPSIG (status));
  } else if (WIFEXITED (status)) {
    uv_signal_stop (handle);
    uv_poll_stop (&priv->poll);
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
      PTRACE_O_EXITKILL | PTRACE_O_TRACESECCOMP | PTRACE_O_TRACEEXEC);

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
