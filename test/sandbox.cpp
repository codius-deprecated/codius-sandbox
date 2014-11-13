#include "sandbox.h"
#include "sandbox-ipc.h"

#include <cppunit/extensions/HelperMacros.h>
#include <string.h>
#include <condition_variable>
#include <mutex>
#include <uv.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <unistd.h>

#ifndef BUILD_PATH
#define BUILD_PATH "./"
#endif

#define strx(s) #s

#define STRINGIFY(s) strx(s)

#define TESTER_BINARY STRINGIFY(BUILD_PATH) "/build/Debug/syscall-tester"

bool operator< (const Sandbox::SyscallCall& first, const Sandbox::SyscallCall& other)
{
  return first.id < other.id;
}

bool operator== (const Sandbox::SyscallCall& first, const Sandbox::SyscallCall& other)
{
  return first.id == other.id;
}

class TestIPC : public SandboxIPC {
public:
  TestIPC(int dupAs) : SandboxIPC (dupAs) {}
  void onReadReady() override {
    char buf[1024];
    ssize_t readSize;

    readSize = read (parent, buf, sizeof (buf)-2);
    buf[readSize] = 0;
    printf ("%s", buf);
  }
};

class TestSandbox : public Sandbox {
public:
  TestSandbox() : Sandbox(),
                  exitStatus(-1) {
    addIPC(std::unique_ptr<TestIPC> (new TestIPC(STDOUT_FILENO)));
    addIPC(std::unique_ptr<TestIPC> (new TestIPC(STDERR_FILENO)));
  }

  SyscallCall handleSyscall(const SyscallCall& call) override {
    history.push_back (call);

    if (remap.find (call.id) != remap.cend()) {
      return remap[call];
    }
    return call;
  }

  void handleIPC(codius_request_t*) override {}

  void handleSignal(int signal) override {}

  void handleExit(int status) override {
    exitStatus = status;
  }

  void waitExit() {
    uv_loop_t* loop = uv_default_loop ();
    while (exitStatus == -1)
      uv_run (loop, UV_RUN_NOWAIT);
  }

  int exitStatus;
  std::vector<SyscallCall> history;
  std::map<SyscallCall, SyscallCall> remap;
};

class SandboxTest : public CppUnit::TestFixture {
  CPPUNIT_TEST_SUITE (SandboxTest);
  CPPUNIT_TEST (testSimpleProgram);
  CPPUNIT_TEST (testExitStatus);
  CPPUNIT_TEST_SUITE_END ();

private:
  std::unique_ptr<TestSandbox> sbox;

public:
    void printHistory()
    {
      std::cout << sbox->history.size() << " calls were made:" << std::endl;
      for (auto i = sbox->history.cbegin(); i != sbox->history.cend(); i++) {
        std::cout << (*i).id << " " << std::hex;
        for (size_t j = 0; j < 6; j++) {
          std::cout << (*i).args[j] << " ";
        }
        std::cout << std::dec << std::endl;
      }
    }

    void setUp()
    {
      sbox = std::unique_ptr<TestSandbox>(new TestSandbox());
    }

    void tearDown()
    {
      sbox.reset (nullptr);
    }

    void _run (int syscall)
    {
      std::map<std::string, std::string> envp;
      char* argv[3];
      argv[0] = strdup (TESTER_BINARY);
      argv[1] = (char*)calloc (sizeof (char), 15);
      sprintf (argv[1], "%d", syscall);
      argv[2] = nullptr;
      sbox->spawn (argv, envp);
      for (size_t i = 0; argv[i]; i++)
        free (argv[i]);
    }

    void testSimpleProgram()
    {
      _run (SYS_exit);
      sbox->waitExit();
      CPPUNIT_ASSERT_EQUAL (0, sbox->exitStatus);
    }

    void testExitStatus()
    {
      _run (SYS_open);
      sbox->waitExit();
      CPPUNIT_ASSERT_EQUAL (2, sbox->exitStatus);
    }

    void testInterceptSyscall()
    {
      _run (SYS_accept);
      bool found = false;
      for (auto i = sbox->history.cbegin(); i != sbox->history.cend(); i++)
        if ((*i).id == SYS_accept)
          found = true;
      CPPUNIT_ASSERT (found);
    }

};


CPPUNIT_TEST_SUITE_REGISTRATION (SandboxTest);
