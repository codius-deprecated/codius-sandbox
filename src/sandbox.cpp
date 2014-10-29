#include "sandbox.h"

#include <errno.h>
#include <vector>
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
#include <cassert>

#include "codius-util.h"
#include "sandbox-ipc.h"

#define PTRACE_EVENT_SECCOMP 7

static void handle_ipc_read (SandboxIPC& ipc, void* user_data);

class SandboxPrivate {
  public:
    SandboxPrivate(Sandbox* d)
      : d (d),
        pid(0),
        entered_main(false),
        scratchAddr(0) {}
    Sandbox* d;
    std::vector<std::unique_ptr<SandboxIPC> > ipcSockets;
    pid_t pid;
    uv_signal_t signal;
    bool entered_main;
    Sandbox::Address scratchAddr;
    Sandbox::Address nextScratchSegment;
    void handleSeccompEvent();
    void handleExecEvent();
    std::string cwd;
};

std::string
Sandbox::getCWD() const
{
  return m_p->cwd;
}

bool
Sandbox::enteredMain() const
{
  return m_p->entered_main;
}

void
SandboxPrivate::handleExecEvent()
{
  if (!entered_main) {
    struct user_regs_struct regs;
    Sandbox::Address stackAddr;
    Sandbox::Address environAddr;
    Sandbox::Address strAddr;
    int argc;

    entered_main = true;

    memset (&regs, 0, sizeof (regs));

    if (ptrace (PTRACE_GETREGS, pid, 0, &regs) < 0) {
      error (EXIT_FAILURE, errno, "Failed to fetch registers on exec");
    }

    stackAddr = regs.rsp;
    d->copyData (stackAddr, sizeof (argc), &argc);
    environAddr = stackAddr + (sizeof (stackAddr) * (argc+2));

    strAddr = d->peekData (environAddr);
    while (strAddr != 0) {
      char buf[1024];
      std::string needle("CODIUS_SCRATCH_BUFFER=");
      d->copyString (strAddr, sizeof (buf), buf);
      environAddr += sizeof (stackAddr);
      if (strncmp (buf, needle.c_str(), needle.length()) == 0) {
        scratchAddr = strAddr + needle.length();
        break;
      }
      strAddr = d->peekData (environAddr);
    }
    assert (scratchAddr);
  }
}

void
Sandbox::addIPC(std::unique_ptr<SandboxIPC>&& ipc)
{
  m_p->ipcSockets.push_back (std::move(ipc));
}

Sandbox::Sandbox()
  : m_p(new SandboxPrivate(this))
{
}

void
Sandbox::kill()
{
  releaseChild (SIGKILL);
}

Sandbox::~Sandbox()
{
  kill();
  delete m_p;
}

void Sandbox::spawn(char **argv, std::map<std::string, std::string>& envp)
{
  SandboxPrivate *priv = m_p;
  SandboxWrap* wrap = new SandboxWrap;
  wrap->priv = priv;
  CallbackIPC::Ptr ipcSocket (new CallbackIPC (3));

  ipcSocket->setCallback (handle_ipc_read, wrap);
  addIPC (std::move (ipcSocket));

  priv->pid = fork();

  char buf[1024];
  getcwd (buf, sizeof (buf));
  priv->cwd = buf;

  if (priv->pid) {
    traceChild();
  } else {
    execChild(argv, envp);
  }
}

void
Sandbox::execChild(char** argv, std::map<std::string, std::string>& envp)
{
  scmp_filter_ctx ctx;

  for(auto i = m_p->ipcSockets.begin(); i != m_p->ipcSockets.end(); i++) {
    if (!(*i)->dup()) {
      error (EXIT_FAILURE, errno, "Could not bind IPC channel across #%d", (*i)->dupAs);
    }
  }

  ptrace (PTRACE_TRACEME, 0, 0);
  raise (SIGSTOP);
  
  prctl (PR_SET_NO_NEW_PRIVS, 1);

  ctx = seccomp_init (SCMP_ACT_TRACE (0));

  seccomp_rule_add (ctx, SCMP_ACT_ALLOW, SCMP_SYS (write), 0);
  seccomp_rule_add (ctx, SCMP_ACT_ALLOW, SCMP_SYS (read), 0);
  seccomp_rule_add (ctx, SCMP_ACT_ALLOW, SCMP_SYS (close), 0);
  seccomp_rule_add (ctx, SCMP_ACT_ALLOW, SCMP_SYS (fstat), 0);
  seccomp_rule_add (ctx, SCMP_ACT_ALLOW, SCMP_SYS (listen), 0);
  seccomp_rule_add (ctx, SCMP_ACT_ALLOW, SCMP_SYS (accept4), 0);

  seccomp_rule_add (ctx, SCMP_ACT_ALLOW, SCMP_SYS (writev), 0);
  seccomp_rule_add (ctx, SCMP_ACT_ALLOW, SCMP_SYS (readv), 0);
  seccomp_rule_add (ctx, SCMP_ACT_ALLOW, SCMP_SYS (sched_yield), 0);
  seccomp_rule_add (ctx, SCMP_ACT_ALLOW, SCMP_SYS (clone), 0);
  seccomp_rule_add (ctx, SCMP_ACT_ALLOW, SCMP_SYS (mprotect), 0);
  seccomp_rule_add (ctx, SCMP_ACT_ALLOW, SCMP_SYS (mmap), 0);
  seccomp_rule_add (ctx, SCMP_ACT_ALLOW, SCMP_SYS (brk), 0);
  seccomp_rule_add (ctx, SCMP_ACT_ALLOW, SCMP_SYS (munmap), 0);
  seccomp_rule_add (ctx, SCMP_ACT_ALLOW, SCMP_SYS (gettid), 0);
  seccomp_rule_add (ctx, SCMP_ACT_ALLOW, SCMP_SYS (clock_getres), 0);
  seccomp_rule_add (ctx, SCMP_ACT_ALLOW, SCMP_SYS (rt_sigprocmask), 0);
  seccomp_rule_add (ctx, SCMP_ACT_ALLOW, SCMP_SYS (rt_sigaction), 0);
  seccomp_rule_add (ctx, SCMP_ACT_ALLOW, SCMP_SYS (ioctl), 0);
  seccomp_rule_add (ctx, SCMP_ACT_ALLOW, SCMP_SYS (eventfd2), 0);
  seccomp_rule_add (ctx, SCMP_ACT_ALLOW, SCMP_SYS (pipe2), 0);
  seccomp_rule_add (ctx, SCMP_ACT_ALLOW, SCMP_SYS (epoll_create1), 0);
  seccomp_rule_add (ctx, SCMP_ACT_ALLOW, SCMP_SYS (futex), 0);
  seccomp_rule_add (ctx, SCMP_ACT_ALLOW, SCMP_SYS (epoll_wait), 0);
  seccomp_rule_add (ctx, SCMP_ACT_ALLOW, SCMP_SYS (epoll_ctl), 0);
  seccomp_rule_add (ctx, SCMP_ACT_ALLOW, SCMP_SYS (getcwd), 0);
  seccomp_rule_add (ctx, SCMP_ACT_ALLOW, SCMP_SYS (set_robust_list), 0);
  seccomp_rule_add (ctx, SCMP_ACT_ALLOW, SCMP_SYS (get_robust_list), 0);
  seccomp_rule_add (ctx, SCMP_ACT_ALLOW, SCMP_SYS (set_tid_address), 0);
  seccomp_rule_add (ctx, SCMP_ACT_ALLOW, SCMP_SYS (arch_prctl), 0); // FIXME: what is this used for?
  seccomp_rule_add (ctx, SCMP_ACT_KILL, SCMP_SYS (ptrace), 0);

  // FIXME: Do these need emulated?
  seccomp_rule_add (ctx, SCMP_ACT_ALLOW, SCMP_SYS (uname), 0);
  seccomp_rule_add (ctx, SCMP_ACT_ALLOW, SCMP_SYS (getrlimit), 0);

  //seccomp_rule_add (ctx, SCMP_ACT_ALLOW, SCMP_SYS (getsockname), 0);
  //seccomp_rule_add (ctx, SCMP_ACT_ALLOW, SCMP_SYS (getsockopt), 0);
  //seccomp_rule_add (ctx, SCMP_ACT_ALLOW, SCMP_SYS (fstat), 0);

  if (0<seccomp_load (ctx))
    error(EXIT_FAILURE, errno, "Could not lock down sandbox");
  seccomp_release (ctx);

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

Sandbox::Word
Sandbox::peekData(Address addr)
{
  return ptrace (PTRACE_PEEKDATA, m_p->pid, addr, NULL);
}

bool
Sandbox::copyData(Address addr, size_t length, void* buf)
{
  for (size_t i = 0; i < length; i += sizeof (Word)) {
    Word ret = peekData (addr+i);
    memcpy (reinterpret_cast<void*>(reinterpret_cast<long>(buf)+i), &ret, sizeof (Word));
  }
  if (errno)
    return false;
  return true;
}

bool
Sandbox::copyString (Address addr, int maxLength, char* buf)
{
  //FIXME: This causes a lot of redundant copying. Whole words are moved around
  //and then only a single byte is taken out of it when we could be operating on
  //bigger chunks of data.
  for (int i = 0; i < maxLength; i++) {
    buf[i] = peekData(addr + i);
    if (buf[i] == 0)
      break;
  }

  if (errno)
    return false;
  return true;
}

void
SandboxPrivate::handleSeccompEvent()
{
  struct user_regs_struct regs;
  
  if (!entered_main)
    return;
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

  d->resetScratch();
  call = d->handleSyscall (call);

  if (call.id == __NR_chdir) {
    std::vector<char> newDir (1024);
    d->copyString (call.args[0], newDir.size(), newDir.data());
    cwd = newDir.data();
  }

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
Sandbox::releaseChild(int signal)
{
  SandboxPrivate *priv = m_p;
  ptrace (PTRACE_SETOPTIONS, priv->pid, 0, 0);
  uv_signal_stop (&priv->signal);
  priv->ipcSockets.clear();
  ptrace (PTRACE_DETACH, m_p->pid, 0, signal);
}

Sandbox::Address
Sandbox::getScratchAddress() const
{
  return m_p->scratchAddr;
}

bool
Sandbox::pokeData(Address addr, Word word)
{
  return ptrace (PTRACE_POKEDATA, m_p->pid, addr, word);
}

//FIXME: Needs some test to make sure we don't go outside the scratch area
Sandbox::Address
Sandbox::writeScratch(size_t length, const char* buf)
{
  Address nextAddr;
  Address curAddr = m_p->nextScratchSegment;
  writeData (curAddr, length, buf);
  nextAddr = m_p->nextScratchSegment + length;
  // Round up to nearest word boundary
  if (nextAddr % sizeof (Address) != 0)
    nextAddr += sizeof (Address) - nextAddr % sizeof (Address);
  m_p->nextScratchSegment = nextAddr;
  return curAddr;
}

void
Sandbox::resetScratch()
{
  m_p->nextScratchSegment = m_p->scratchAddr;
}

bool
Sandbox::writeData (Address addr, size_t length, const char* buf)
{
  for (size_t i = 0; i < length; i++) {
    pokeData (m_p->scratchAddr + i, buf[i]);
  }
  if (errno)
    return false;
  return true;
}

static void
handle_trap(uv_signal_t *handle, int signum)
{
  SandboxWrap* wrap = static_cast<SandboxWrap*>(handle->data);
  SandboxPrivate* priv = wrap->priv;
  int status = 0;

  waitpid (priv->pid, &status, WNOHANG);

  if (WIFSTOPPED (status)) {
    if (WSTOPSIG (status) == SIGTRAP) {
      int s = ((status >> 8) & ~SIGTRAP) >> 8;
      if (s == PTRACE_EVENT_SECCOMP) {
        priv->handleSeccompEvent();
        ptrace (PTRACE_CONT, priv->pid, 0, 0);
      } else if (s == PTRACE_EVENT_EXIT) {
        ptrace (PTRACE_GETEVENTMSG, priv->pid, 0, &status);
        priv->d->handleExit (WEXITSTATUS (status));
        priv->d->releaseChild(0);
      } else if (s == PTRACE_EVENT_EXEC) {
        priv->handleExecEvent();
        ptrace (PTRACE_CONT, priv->pid, 0, 0);
      } else {
        assert(false);
      }
    } else {
      priv->d->handleSignal (WSTOPSIG (status));
      ptrace (PTRACE_CONT, priv->pid, 0, WSTOPSIG (status));
    }
  } else if (WIFCONTINUED (status)) {
    ptrace (PTRACE_CONT, priv->pid, 0, 0);
  } else if (WIFSIGNALED (status)) {
    assert (false);
  } else if (WIFEXITED (status)) {
    assert (false);
  }
}

static void
handle_ipc_read (SandboxIPC& ipc, void* data)
{
  codius_request_t* request;
  codius_result_t* result;
  SandboxWrap* wrap = static_cast<SandboxWrap*>(data);
  SandboxPrivate* priv = wrap->priv;

  request = codius_read_request (ipc.parent);
  if (request == NULL)
    error(EXIT_FAILURE, errno, "couldnt read IPC header");

  result = priv->d->handleIPC(request);
  codius_request_free (request);

  if (result) {
    codius_write_result (ipc.parent, result);
  } else {
    result = codius_result_new ();
  }
  codius_result_free (result);
}

void
Sandbox::traceChild()
{
  SandboxPrivate* priv = m_p;
  uv_loop_t* loop = uv_default_loop ();
  int status = 0;

  ptrace (PTRACE_ATTACH, priv->pid, 0, 0);
  waitpid (priv->pid, &status, 0);
  ptrace (PTRACE_SETOPTIONS, priv->pid, 0,
      PTRACE_O_EXITKILL | PTRACE_O_TRACEEXIT | PTRACE_O_TRACESECCOMP | PTRACE_O_TRACEEXEC);

  uv_signal_init (loop, &priv->signal);
  SandboxWrap* wrap = new SandboxWrap;
  wrap->priv = priv;
  priv->signal.data = wrap;
  uv_signal_start (&priv->signal, handle_trap, SIGCHLD);

  for (auto i = priv->ipcSockets.begin(); i != priv->ipcSockets.end(); i++)
    (*i)->startPoll(loop);

  ptrace (PTRACE_CONT, priv->pid, 0, 0);
}
