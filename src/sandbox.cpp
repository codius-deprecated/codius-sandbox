#include "sandbox.h"

#include <iostream>
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
#include "vfs.h"
#include <dirent.h>
#include <sys/types.h>
#include <iostream>

#include "codius-util.h"
#include "sandbox-ipc.h"

#ifndef PTRACE_EVENT_SECCOMP
#define PTRACE_EVENT_SECCOMP 7
#endif

#ifndef PTRACE_O_EXITKILL
#define PTRACE_O_EXITKILL (1 << 20)
#endif

#ifndef PTRACE_O_TRACESECCOMP
#define PTRACE_O_TRACESECCOMP (1 << PTRACE_EVENT_SECCOMP)
#endif

static void handle_ipc_read (SandboxIPC& ipc, void* user_data);

class SandboxPrivate {
  public:
    SandboxPrivate(Sandbox* d)
      : d (d),
        pid(0),
        entered_main(false),
        scratchAddr(0),
        vfs(new VFS(d)) {}
    Sandbox* d;
    std::vector<std::unique_ptr<SandboxIPC> > ipcSockets;
    pid_t pid;
    uv_signal_t signal;
    bool entered_main;
    Sandbox::Address scratchAddr;
    Sandbox::Address nextScratchSegment;
    void handleSeccompEvent(pid_t pid);
    void handleExecEvent(pid_t pid);
    std::vector<int> openFiles;
    std::unique_ptr<VFS> vfs;
};

bool
Sandbox::enteredMain() const
{
  return m_p->entered_main;
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

pid_t
Sandbox::fork()
{
  SandboxPrivate *priv = m_p;
  SandboxWrap* wrap = new SandboxWrap;
  wrap->priv = priv;
  CallbackIPC::Ptr ipcSocket (new CallbackIPC (3));

  ipcSocket->setCallback (handle_ipc_read, wrap);
  addIPC (std::move (ipcSocket));

  priv->pid = ::fork();
  if (!priv->pid)
    setupSandboxing();
  return priv->pid;
}

Sandbox::Word
Sandbox::peekData(pid_t pid, Address addr)
{
  Word w = ptrace (PTRACE_PEEKDATA, pid, addr, NULL);
  assert (errno == 0);
  return w;
}

bool
Sandbox::copyData(pid_t pid, Address addr, size_t length, void* buf)
{
  for (size_t i = 0; i < length; i += sizeof (Word)) {
    Word ret = peekData (pid, addr+i);
    memcpy (reinterpret_cast<void*>(reinterpret_cast<long>(buf)+i), &ret, sizeof (Word));
  }
  if (errno)
    return false;
  return true;
}

bool
Sandbox::copyString (pid_t pid, Address addr, size_t maxLength, char* buf)
{
  //FIXME: This causes a lot of redundant copying. Whole words are moved around
  //and then only a single byte is taken out of it when we could be operating on
  //bigger chunks of data.
  bool endString = false;
  size_t i;
  for (i = 0; !endString && maxLength - i > sizeof (Word); i += sizeof (Word)) {
    Word d = peekData(pid, addr + i);
    for (size_t j = 0; j < sizeof (Word) && i + j < maxLength; j++) {
      buf[i + j] = ((char*)(&d))[j];
      if (buf[i + j] == 0) {
        endString = true;
        break;
      }
    }
  }

  if (i != maxLength && !endString) {
    Word d = peekData (pid, addr + i);
    for (size_t j = 0; j < sizeof (Word) && i + j < maxLength; j++) {
      buf[i + j] = ((char*)(&d))[j];
    }
  }

  if (errno)
    return false;
  return true;
}

void
Sandbox::setScratchAddress(Address addr)
{
  m_p->scratchAddr = addr;
}

void
SandboxPrivate::handleSeccompEvent(pid_t pid)
{
  struct user_regs_struct regs;
  
  if (!entered_main)
    return;
  memset (&regs, 0, sizeof (regs));
  if (ptrace (PTRACE_GETREGS, pid, 0, &regs) < 0) {
    error (EXIT_FAILURE, errno, "Failed to fetch registers");
  }

  Sandbox::SyscallCall call (pid);

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
  call = Sandbox::SyscallCall (d->handleSyscall (call));
  call = Sandbox::SyscallCall (vfs->handleSyscall (call));

#ifdef __i386__
  regs.orig_eax = call.id;
  regs.ebx = call.args[0];
  regs.ecx = call.args[1];
  regs.edx = call.args[2];
  regs.esi = call.args[3];
  regs.edi = call.args[4];
  regs.ebp = call.args[5];
  regs.eax = call.returnVal;
#else
  regs.orig_rax = call.id;
  regs.rdi = call.args[0];
  regs.rsi = call.args[1];
  regs.rdx = call.args[2];
  regs.rcx = call.args[3];
  regs.r8 = call.args[4];
  regs.r9 = call.args[5];
  regs.rax = call.returnVal;
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
Sandbox::pokeData(pid_t pid, Address addr, Word word)
{
  return ptrace (PTRACE_POKEDATA, pid, addr, word);
}

//FIXME: Needs some test to make sure we don't go outside the scratch area
Sandbox::Address
Sandbox::writeScratch(pid_t pid, size_t length, const char* buf)
{
  Address nextAddr;
  Address curAddr = m_p->nextScratchSegment;
  writeData (pid, curAddr, length, buf);
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
Sandbox::writeData (pid_t pid, Address addr, size_t length, const char* buf)
{
  size_t i;
  for (i = 0; length - i > sizeof (Word); i += sizeof (Word)) {
    Word d;
    for (size_t j = 0; j < sizeof (Word) && i+j < length; j++) {
      ((char*)(&d))[j] = buf[i+j];
    }
    pokeData (pid, addr + i, d);
  }
  if (i != length) {
    Word d = peekData (pid, addr + i);
    for (size_t j = 0; j < sizeof (Word) && i + j < length; j++) {
      ((char*)(&d))[j] = buf[i+j];
    }
    pokeData (pid, addr + i, d);
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
  pid_t pid;

  while (true) {
    pid = waitpid (-priv->pid, &status, WNOHANG | __WALL);

    if (pid == 0)
      break;

    if (WIFSTOPPED (status)) {
      if (WSTOPSIG (status) == SIGTRAP) {
        int s = ((status >> 8) & ~SIGTRAP) >> 8;
        if (s == PTRACE_EVENT_SECCOMP) {
          priv->handleSeccompEvent(pid);
          ptrace (PTRACE_CONT, pid, 0, 0);
        } else if (s == PTRACE_EVENT_EXIT) {
          if (pid == priv->pid) {
            ptrace (PTRACE_GETEVENTMSG, pid, 0, &status);
            if (WIFSIGNALED (status)) {
              if (WTERMSIG (status) == SIGSYS) {
                struct user_regs_struct regs;
                ptrace (PTRACE_GETREGS, pid, 0, &regs);
                std::cout << "died on bad syscall " << regs.orig_rax << std::endl;
              }
              priv->d->handleSignal (WTERMSIG (status));
              priv->d->handleExit (WTERMSIG (status));
            } else {
              assert (WIFEXITED (status));
              priv->d->handleExit (WEXITSTATUS (status));
            }
            priv->d->releaseChild(0);
          } else {
            ptrace (PTRACE_CONT, pid, 0, 0);
          }
        } else if (s == PTRACE_EVENT_EXEC) {
          priv->d->handleExecEvent(pid);
          ptrace (PTRACE_CONT, pid, 0, 0);
        } else if (s == PTRACE_EVENT_CLONE) {
          pid_t childPID;
          ptrace (PTRACE_GETEVENTMSG, pid, 0, &childPID);
          ptrace (PTRACE_SETOPTIONS, childPID, 0,
              PTRACE_O_EXITKILL | PTRACE_O_TRACEEXIT | PTRACE_O_TRACESECCOMP | PTRACE_O_TRACEEXEC | PTRACE_O_TRACECLONE);
          ptrace (PTRACE_CONT, childPID, 0, 0);
          ptrace (PTRACE_CONT, pid, 0, 0);
        } else {
          assert(false);
        }
      } else {
        priv->d->handleSignal (WSTOPSIG (status));
        ptrace (PTRACE_CONT, pid, 0, WSTOPSIG (status));
      }
    } else if (WIFCONTINUED (status)) {
      ptrace (PTRACE_CONT, pid, 0, 0);
    } else if (WIFSIGNALED (status) && pid == priv->pid) {
      assert (false);
    } else if (WIFEXITED (status) && pid == priv->pid) {
      assert (false);
    }
  }
}

static void
handle_ipc_read (SandboxIPC& ipc, void* data)
{
  codius_request_t* request;
  SandboxWrap* wrap = static_cast<SandboxWrap*>(data);
  SandboxPrivate* priv = wrap->priv;

  request = codius_read_request (ipc.parent);
  if (request == NULL)
    error(EXIT_FAILURE, errno, "couldnt read IPC header");

  priv->d->handleIPC(request);
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
      PTRACE_O_EXITKILL | PTRACE_O_TRACEEXIT | PTRACE_O_TRACESECCOMP | PTRACE_O_TRACEEXEC | PTRACE_O_TRACECLONE);

  uv_signal_init (loop, &priv->signal);
  SandboxWrap* wrap = new SandboxWrap;
  wrap->priv = priv;
  priv->signal.data = wrap;

  for (auto i = priv->ipcSockets.begin(); i != priv->ipcSockets.end(); i++)
    (*i)->startPoll(loop);

  uv_signal_start (&priv->signal, handle_trap, SIGCHLD);
  ptrace (PTRACE_CONT, priv->pid, 0, 0);
}

VFS&
Sandbox::getVFS() const
{
  return *m_p->vfs;
}

void
Sandbox::setupSandboxing()
{
  scmp_filter_ctx ctx;
  std::vector<int> permittedFDs (m_p->ipcSockets.size());
  std::vector<int> unusedFDs;

  for(auto i = m_p->ipcSockets.begin(); i != m_p->ipcSockets.end(); i++) {
    if (!(*i)->dup()) {
      error (EXIT_FAILURE, errno, "Could not bind IPC channel across #%d", (*i)->dupAs);
    }

    permittedFDs.push_back ((*i)->dupAs);
  }

  DIR* dirp = opendir ("/proc/self/fd/");
  struct dirent* dp;
  do {
    if ((dp = readdir (dirp)) != NULL) {
      bool found = false;
      int fdnum;
      char* end = NULL;

      fdnum = strtol (dp->d_name, &end, 10);

      if (end == dp->d_name && fdnum == 0) {
        continue;
      }

      for (auto i = permittedFDs.cbegin(); i != permittedFDs.cend(); i++) {
        if (*i == fdnum) {
          found = true;
          break;
        }
      }

      if (!found) {
        unusedFDs.push_back (fdnum);
      }
    }
  } while (dp != NULL);
  closedir (dirp);

  for (auto i = unusedFDs.cbegin(); i != unusedFDs.cend(); i++) {
    close (*i);
  }

  setpgid (0, 0);

  ptrace (PTRACE_TRACEME, 0, 0);
  raise (SIGSTOP);
  
  prctl (PR_SET_NO_NEW_PRIVS, 1);

  ctx = seccomp_init (SCMP_ACT_KILL);

  // A duplicate in case the seccomp_init() call is accidentally modified
  seccomp_rule_add (ctx, SCMP_ACT_KILL, SCMP_SYS (ptrace), 0);

  // This is actually caught via PTRACE_EVENT_EXEC
  seccomp_rule_add (ctx, SCMP_ACT_ALLOW, SCMP_SYS (execve), 0);
  seccomp_rule_add (ctx, SCMP_ACT_ALLOW, SCMP_SYS (clone), 0);

  // Used to track chdir calls
  seccomp_rule_add (ctx, SCMP_ACT_TRACE (0), SCMP_SYS (chdir), 0);
  seccomp_rule_add (ctx, SCMP_ACT_TRACE (0), SCMP_SYS (fchdir), 0);

  // These interact with the VFS layer
  seccomp_rule_add (ctx, SCMP_ACT_TRACE (0), SCMP_SYS (open), 0);
  seccomp_rule_add (ctx, SCMP_ACT_TRACE (0), SCMP_SYS (access), 0);
  seccomp_rule_add (ctx, SCMP_ACT_TRACE (0), SCMP_SYS (openat), 0);
  seccomp_rule_add (ctx, SCMP_ACT_TRACE (0), SCMP_SYS (stat), 0);
  seccomp_rule_add (ctx, SCMP_ACT_TRACE (0), SCMP_SYS (lstat), 0);
  seccomp_rule_add (ctx, SCMP_ACT_TRACE (0), SCMP_SYS (getcwd), 0);
  seccomp_rule_add (ctx, SCMP_ACT_TRACE (0), SCMP_SYS (readlink), 0);

#define VFS_FILTER(x) seccomp_rule_add (ctx, \
                                        SCMP_ACT_TRACE (0), \
                                        SCMP_SYS (x), 1, \
                                        SCMP_A0 (SCMP_CMP_GE, VFS::firstVirtualFD)); \
                      seccomp_rule_add (ctx, \
                                        SCMP_ACT_ALLOW, \
                                        SCMP_SYS (x), 1, \
                                        SCMP_A0 (SCMP_CMP_LT, VFS::firstVirtualFD));
  VFS_FILTER (read);
  VFS_FILTER (close);
  VFS_FILTER (ioctl);
  VFS_FILTER (fstat);
  VFS_FILTER (lseek);
  VFS_FILTER (write);
  VFS_FILTER (getdents);
#ifdef __NR_readdir
  VFS_FILTER (readdir);
#endif // __NR_readdir
  VFS_FILTER (getdents64);
  VFS_FILTER (readv);
  VFS_FILTER (writev);

#undef VFS_FILTER

  // This needs its arguments sanitized
  seccomp_rule_add (ctx, SCMP_ACT_TRACE (0), SCMP_SYS (fcntl), 0);

  // These are traced to implement socket remapping
  seccomp_rule_add (ctx, SCMP_ACT_TRACE (0), SCMP_SYS (socket), 0);
  seccomp_rule_add (ctx, SCMP_ACT_TRACE (0), SCMP_SYS (connect), 0);
  seccomp_rule_add (ctx, SCMP_ACT_TRACE (0), SCMP_SYS (bind), 0);
  seccomp_rule_add (ctx, SCMP_ACT_TRACE (0), SCMP_SYS (setsockopt), 0);
  seccomp_rule_add (ctx, SCMP_ACT_TRACE (0), SCMP_SYS (getsockname), 0);
  seccomp_rule_add (ctx, SCMP_ACT_TRACE (0), SCMP_SYS (getpeername), 0);
  seccomp_rule_add (ctx, SCMP_ACT_TRACE (0), SCMP_SYS (getsockopt), 0);

  // These need their return values faked in some way
  seccomp_rule_add (ctx, SCMP_ACT_TRACE (0), SCMP_SYS (uname), 0);
  seccomp_rule_add (ctx, SCMP_ACT_TRACE (0), SCMP_SYS (getrlimit), 0);
  seccomp_rule_add (ctx, SCMP_ACT_TRACE (0), SCMP_SYS (getuid), 0);
  seccomp_rule_add (ctx, SCMP_ACT_TRACE (0), SCMP_SYS (getgid), 0);
  seccomp_rule_add (ctx, SCMP_ACT_TRACE (0), SCMP_SYS (geteuid), 0);
  seccomp_rule_add (ctx, SCMP_ACT_TRACE (0), SCMP_SYS (getegid), 0);
  seccomp_rule_add (ctx, SCMP_ACT_TRACE (0), SCMP_SYS (getppid), 0);
  seccomp_rule_add (ctx, SCMP_ACT_TRACE (0), SCMP_SYS (getpgrp), 0);
  seccomp_rule_add (ctx, SCMP_ACT_TRACE (0), SCMP_SYS (getgroups), 0);
  seccomp_rule_add (ctx, SCMP_ACT_TRACE (0), SCMP_SYS (getresuid), 0);
  seccomp_rule_add (ctx, SCMP_ACT_TRACE (0), SCMP_SYS (getresgid), 0);
  seccomp_rule_add (ctx, SCMP_ACT_TRACE (0), SCMP_SYS (capget), 0);
  seccomp_rule_add (ctx, SCMP_ACT_TRACE (0), SCMP_SYS (gettid), 0);

  // All of these are allowed because they either:
  // * Can't cause any harm outside the sandbox
  // * Require some file descriptor from a previously-sanitized call to i.e.
  // open()
  seccomp_rule_add (ctx, SCMP_ACT_ALLOW, SCMP_SYS (fsync), 0);
  seccomp_rule_add (ctx, SCMP_ACT_ALLOW, SCMP_SYS (fdatasync), 0);
  seccomp_rule_add (ctx, SCMP_ACT_ALLOW, SCMP_SYS (sync), 0);
  seccomp_rule_add (ctx, SCMP_ACT_ALLOW, SCMP_SYS (poll), 0);
  seccomp_rule_add (ctx, SCMP_ACT_ALLOW, SCMP_SYS (mmap), 0);
  seccomp_rule_add (ctx, SCMP_ACT_ALLOW, SCMP_SYS (mprotect), 0);
  seccomp_rule_add (ctx, SCMP_ACT_ALLOW, SCMP_SYS (munmap), 0);
  seccomp_rule_add (ctx, SCMP_ACT_ALLOW, SCMP_SYS (madvise), 0);
  seccomp_rule_add (ctx, SCMP_ACT_ALLOW, SCMP_SYS (brk), 0);
  seccomp_rule_add (ctx, SCMP_ACT_ALLOW, SCMP_SYS (rt_sigaction), 0);
  seccomp_rule_add (ctx, SCMP_ACT_ALLOW, SCMP_SYS (rt_sigprocmask), 0);
  seccomp_rule_add (ctx, SCMP_ACT_ALLOW, SCMP_SYS (select), 0);
  seccomp_rule_add (ctx, SCMP_ACT_ALLOW, SCMP_SYS (sched_yield), 0);
  seccomp_rule_add (ctx, SCMP_ACT_ALLOW, SCMP_SYS (getpid), 0);
  seccomp_rule_add (ctx, SCMP_ACT_ALLOW, SCMP_SYS (accept), 0);
  seccomp_rule_add (ctx, SCMP_ACT_ALLOW, SCMP_SYS (listen), 0);
  seccomp_rule_add (ctx, SCMP_ACT_ALLOW, SCMP_SYS (exit), 0);
  seccomp_rule_add (ctx, SCMP_ACT_ALLOW, SCMP_SYS (gettimeofday), 0);
  seccomp_rule_add (ctx, SCMP_ACT_ALLOW, SCMP_SYS (tkill), 0);
  seccomp_rule_add (ctx, SCMP_ACT_ALLOW, SCMP_SYS (epoll_create), 0);
  seccomp_rule_add (ctx, SCMP_ACT_ALLOW, SCMP_SYS (restart_syscall), 0);
  seccomp_rule_add (ctx, SCMP_ACT_ALLOW, SCMP_SYS (clock_gettime), 0);
  seccomp_rule_add (ctx, SCMP_ACT_ALLOW, SCMP_SYS (clock_getres), 0);
  seccomp_rule_add (ctx, SCMP_ACT_ALLOW, SCMP_SYS (clock_nanosleep), 0);
  seccomp_rule_add (ctx, SCMP_ACT_ALLOW, SCMP_SYS (gettid), 0);
  seccomp_rule_add (ctx, SCMP_ACT_ALLOW, SCMP_SYS (ioctl), 0);
  seccomp_rule_add (ctx, SCMP_ACT_ALLOW, SCMP_SYS (nanosleep), 0);
  seccomp_rule_add (ctx, SCMP_ACT_ALLOW, SCMP_SYS (exit_group), 0);
  seccomp_rule_add (ctx, SCMP_ACT_ALLOW, SCMP_SYS (epoll_wait), 0);
  seccomp_rule_add (ctx, SCMP_ACT_ALLOW, SCMP_SYS (epoll_ctl), 0);
  seccomp_rule_add (ctx, SCMP_ACT_ALLOW, SCMP_SYS (tgkill), 0);
  seccomp_rule_add (ctx, SCMP_ACT_ALLOW, SCMP_SYS (pselect6), 0);
  seccomp_rule_add (ctx, SCMP_ACT_ALLOW, SCMP_SYS (ppoll), 0);
  seccomp_rule_add (ctx, SCMP_ACT_ALLOW, SCMP_SYS (arch_prctl), 0);
  seccomp_rule_add (ctx, SCMP_ACT_ALLOW, SCMP_SYS (prctl), 0);
  seccomp_rule_add (ctx, SCMP_ACT_ALLOW, SCMP_SYS (set_robust_list), 0);
  seccomp_rule_add (ctx, SCMP_ACT_ALLOW, SCMP_SYS (get_robust_list), 0);
  seccomp_rule_add (ctx, SCMP_ACT_ALLOW, SCMP_SYS (epoll_pwait), 0);
  seccomp_rule_add (ctx, SCMP_ACT_ALLOW, SCMP_SYS (accept4), 0);
  seccomp_rule_add (ctx, SCMP_ACT_ALLOW, SCMP_SYS (eventfd2), 0);
  seccomp_rule_add (ctx, SCMP_ACT_ALLOW, SCMP_SYS (epoll_create1), 0);
  seccomp_rule_add (ctx, SCMP_ACT_ALLOW, SCMP_SYS (pipe2), 0);
  seccomp_rule_add (ctx, SCMP_ACT_ALLOW, SCMP_SYS (futex), 0);
  seccomp_rule_add (ctx, SCMP_ACT_ALLOW, SCMP_SYS (set_tid_address), 0);
  seccomp_rule_add (ctx, SCMP_ACT_ALLOW, SCMP_SYS (set_thread_area), 0);

  if (0<seccomp_load (ctx))
    error(EXIT_FAILURE, errno, "Could not lock down sandbox");
  seccomp_release (ctx);
}

void
Sandbox::setEnteredMain(bool entered)
{
  m_p->entered_main = entered;
}
