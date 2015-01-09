#include "sandbox-test.h"

#include <cppunit/extensions/HelperMacros.h>
#include <string.h>
#include <cassert>
#include <condition_variable>
#include <mutex>
#include <uv.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <unistd.h>

#include "debug.h"

bool operator< (const Sandbox::SyscallCall& first, const Sandbox::SyscallCall& other)
{
  return first.id < other.id;
}

bool operator== (const Sandbox::SyscallCall& first, const Sandbox::SyscallCall& other)
{
  return first.id == other.id;
}

TestSandbox::TestSandbox() :
  ThreadSandbox(),
  exitStatus(-1)
{
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
  Debug() << "exit with " << status;
  exitStatus = status;
}

void
TestSandbox::handleSignal(int signal)
{}

void
TestSandbox::waitExit()
{
  uv_loop_t* loop = uv_default_loop ();
  while (exitStatus == -1)
    uv_run (loop, UV_RUN_ONCE);
}

void
SandboxTest::setUp()
{
  sbox = makeSandbox();
}

void
SandboxTest::tearDown()
{
  sbox.reset (nullptr);
}

std::unique_ptr<TestSandbox>
SandboxTest::makeSandbox()
{
  return std::unique_ptr<TestSandbox> (new TestSandbox());
}
