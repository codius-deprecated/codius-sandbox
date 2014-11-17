#include "sandbox-ipc.h"
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

void
ExceptionIPC::onReadReady()
{
  char buf[2048];
  int lineNum;
  char filename[1024];
  char message[1024];
  ssize_t readCount;
  memset (filename, 0, sizeof (filename));
  memset (message, 0, sizeof (message));

  readCount = read (parent, buf, sizeof (buf));
  assert (readCount <= (ssize_t)sizeof (buf));
  sscanf(buf, "%s\t%s\t%d", message, filename, &lineNum);
  CppUnit::Asserter::fail(message, CppUnit::SourceLine(filename, lineNum));
}

void
TestIPC::onReadReady()
{
  char buf[1024];
  ssize_t readSize;

  readSize = read (parent, buf, sizeof (buf)-2);
  buf[readSize] = 0;
  Debug() << buf;
}

TestSandbox::TestSandbox() :
  ThreadSandbox(),
  exitStatus(-1)
{
    addIPC(std::unique_ptr<TestIPC> (new TestIPC(STDOUT_FILENO)));
    addIPC(std::unique_ptr<TestIPC> (new TestIPC(STDERR_FILENO)));
    addIPC(std::unique_ptr<ExceptionIPC> (new ExceptionIPC(42)));
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
TestSandbox::handleIPC(codius_request_t* request)
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
