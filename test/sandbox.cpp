#include "sandbox-ipc.h"
#include "sandbox-test.h"

#include <cppunit/extensions/HelperMacros.h>
#include <string.h>
#include <condition_variable>
#include <mutex>
#include <uv.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <unistd.h>

bool operator< (const Sandbox::SyscallCall& first, const Sandbox::SyscallCall& other)
{
  return first.id < other.id;
}

bool operator== (const Sandbox::SyscallCall& first, const Sandbox::SyscallCall& other)
{
  return first.id == other.id;
}

void
TestIPC::onReadReady()
{
  char buf[1024];
  ssize_t readSize;

  readSize = read (parent, buf, sizeof (buf)-2);
  buf[readSize] = 0;
  printf ("%s", buf);
}

TestSandbox::TestSandbox() :
  ThreadSandbox(),
  exitStatus(-1)
{
    addIPC(std::unique_ptr<TestIPC> (new TestIPC(STDOUT_FILENO)));
    addIPC(std::unique_ptr<TestIPC> (new TestIPC(STDERR_FILENO)));
}

Sandbox::SyscallCall
TestSandbox::handleSyscall(const SyscallCall& call)
{
  history.push_back (call);

  if (remap.find (call.id) != remap.cend()) {
    return remap[call];
  }
  return call;
}

void
TestSandbox::handleExit(int status)
{
  exitStatus = status;
}

void
TestSandbox::handleSignal(int signal)
{}

void
TestSandbox::handleIPC(codius_request_t* request)
{}

void
TestSandbox::waitExit()
{
  uv_loop_t* loop = uv_default_loop ();
  while (exitStatus == -1)
    uv_run (loop, UV_RUN_NOWAIT);
}
