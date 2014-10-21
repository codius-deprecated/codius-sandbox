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
#include "sandbox-ipc.h"

#define ORIG_EAX 11
#define PTRACE_EVENT_SECCOMP 7

static void handle_ipc_read (SandboxIPC& ipc, void* user_data);
static void handle_stdio_read (SandboxIPC& ipc, void* user_data);

struct SandboxWrap {
  SandboxPrivate* priv;
};

class SandboxPrivate {
  public:
    SandboxPrivate(Sandbox* d)
      : d (d),
        pid(0),
        entered_main(false),
        scratchAddr(0) {}
    Sandbox* d;
    std::unique_ptr<SandboxIPC> ipcSocket;
    std::unique_ptr<SandboxIPC> stdoutSocket;
    std::unique_ptr<SandboxIPC> stderrSocket;
    pid_t pid;
    uv_signal_t signal;
    bool entered_main;
    Sandbox::Address scratchAddr;
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
  delete m_p;
}

void Sandbox::spawn(char **argv)
{
  SandboxPrivate *priv = m_p;
  SandboxWrap* wrap = new SandboxWrap;

  wrap->priv = priv;

  priv->ipcSocket = std::unique_ptr<SandboxIPC>(new SandboxIPC (3));
  priv->stdoutSocket = std::unique_ptr<SandboxIPC>(new SandboxIPC (STDOUT_FILENO));
  priv->stderrSocket = std::unique_ptr<SandboxIPC>(new SandboxIPC (STDERR_FILENO));

  priv->ipcSocket->setCallback (handle_ipc_read, wrap);
  priv->stdoutSocket->setCallback(handle_stdio_read, wrap);
  priv->stderrSocket->setCallback(handle_stdio_read, wrap);

  priv->pid = fork();

  if (priv->pid) {
    traceChild();
  } else {
    execChild(argv);
  }
}

void
Sandbox::execChild(char** argv)
{
  scmp_filter_ctx ctx;

  if (!m_p->ipcSocket->dup()) {
    error (EXIT_FAILURE, errno, "Could not bind IPC channel");
  }

  if (!m_p->stdoutSocket->dup()) {
    error (EXIT_FAILURE, errno, "Could not bind stdout channel");
  }

  if (!m_p->stderrSocket->dup()) {
    error (EXIT_FAILURE, errno, "Could not bind stderr channel");
  }

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

Sandbox::Word
Sandbox::peekData(Address addr)
{
  return ptrace (PTRACE_PEEKDATA, m_p->pid, addr, NULL);
}

bool
Sandbox::copyData(Address addr, size_t length, void* buf)
{
  for (int i = 0; i < length; i += sizeof (Word)) {
    Word ret = peekData (addr+i);
    memcpy (buf+i, &ret, sizeof (Word));
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
Sandbox::releaseChild(int signal)
{
  SandboxPrivate *priv = m_p;
  ptrace (PTRACE_SETOPTIONS, priv->pid, 0, 0);
  uv_signal_stop (&priv->signal);
  priv->ipcSocket.reset(nullptr);
  priv->stdoutSocket.reset(nullptr);
  priv->stderrSocket.reset(nullptr);
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

bool
Sandbox::writeScratch(size_t length, const char* buf)
{
  return writeData (m_p->scratchAddr, length, buf);
}

bool
Sandbox::writeData (Address addr, size_t length, const char* buf)
{
  for (int i = 0; i < length; i++) {
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

  if (WIFSTOPPED (status) && WSTOPSIG (status) == SIGTRAP) {
    int s = status >> 8;
    if (s == (SIGTRAP | PTRACE_EVENT_SECCOMP << 8)) {
      priv->handleSeccompEvent();
    } else if (s == (SIGTRAP | PTRACE_EVENT_EXIT << 8)) {
      ptrace (PTRACE_GETEVENTMSG, priv->pid, 0, &status);
      uv_signal_stop (handle);
      priv->ipcSocket->stopPoll();
      priv->stdoutSocket->stopPoll();
      priv->stderrSocket->stopPoll();
      priv->d->handleExit (WEXITSTATUS (status));
    } else if (s == (SIGTRAP | PTRACE_EVENT_EXEC << 8)) {
      if (!priv->entered_main) {
        struct user_regs_struct regs;
        Sandbox::Address stackAddr;
        Sandbox::Address environAddr;
        Sandbox::Address strAddr;
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
  } else if (WIFSTOPPED (status)) {
    priv->d->handleSignal (WSTOPSIG (status));
  } else if (WIFSIGNALED (status)) {
    priv->d->handleSignal (WTERMSIG (status));
  } else if (WIFEXITED (status)) {
    uv_signal_stop (handle);
    priv->ipcSocket->stopPoll();
    priv->stdoutSocket->stopPoll();
    priv->stderrSocket->stopPoll();
    priv->d->handleExit (WEXITSTATUS (status));
  }
  ptrace (PTRACE_CONT, priv->pid, 0, WSTOPSIG (status));
}

static void
handle_stdio_read (SandboxIPC& ipc, void* data)
{
  std::vector<char> buf(2048);
  SandboxWrap* wrap = static_cast<SandboxWrap*>(data);
  SandboxPrivate* priv = wrap->priv;
  int bytesRead;

  if ((bytesRead = read (ipc.parent, buf.data(), buf.size()))<0) {
    error (EXIT_FAILURE, errno, "Couldn't read stderr");
  }

  buf.resize (bytesRead);

  std::cout << "stderr: " << buf.data() << std::endl;
}

static void
handle_ipc_read (SandboxIPC& ipc, void* data)
{
  std::vector<char> buf;
  SandboxWrap* wrap = static_cast<SandboxWrap*>(data);
  SandboxPrivate* priv = wrap->priv;
  codius_rpc_header_t header;
  memset (&header, 0, sizeof (header));

  if (read (ipc.parent, &header, sizeof (header)) < 0)
    error(EXIT_FAILURE, errno, "couldnt read IPC header");
  if (header.magic_bytes != CODIUS_MAGIC_BYTES)
    error(EXIT_FAILURE, errno, "Got bad magic header via IPC");
  buf.resize (header.size);
  read (ipc.parent, buf.data(), buf.size());
  buf[buf.size()] = 0;
  priv->d->handleIPC(buf);
  header.size = 1;
  write (ipc.parent, &header, sizeof (header));
  write (ipc.parent, "", 1);
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
      PTRACE_O_EXITKILL | PTRACE_O_TRACESECCOMP | PTRACE_O_TRACEEXEC);

  uv_signal_init (loop, &priv->signal);
  SandboxWrap* wrap = new SandboxWrap;
  wrap->priv = priv;
  priv->signal.data = wrap;
  uv_signal_start (&priv->signal, handle_trap, SIGCHLD);

  priv->ipcSocket->startPoll (loop);
  priv->stdoutSocket->startPoll (loop);
  priv->stderrSocket->startPoll (loop);

  ptrace (PTRACE_CONT, priv->pid, 0, 0);
}

void
Sandbox::handleExit(int status)
{}
