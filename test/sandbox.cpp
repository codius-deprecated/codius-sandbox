#include "sandbox.h"

#include <cppunit/extensions/HelperMacros.h>
#include <string.h>
#include <condition_variable>
#include <mutex>
#include <uv.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <unistd.h>

class TestSandbox : public Sandbox {
public:
  TestSandbox() : Sandbox(),
                  exitStatus(-1) {
  }
  SyscallCall handleSyscall(const SyscallCall& call) override {
    if (redirection.id == 0)
      return call;
    return redirection;
  }

  codius_result_t* handleIPC(codius_request_t*) override {return NULL;}

  void handleSignal(int signal) override {}

  void handleExit(int status) override {
    exitStatus = status;
    exited.notify_all();
  }

  void waitExit() {
    std::unique_lock<std::mutex> lk(exitLock);
    uv_loop_t* loop = uv_default_loop ();
    while (exitStatus == -1)
      uv_run (loop, UV_RUN_NOWAIT);
    exited.wait_for (lk, std::chrono::seconds(1), []{return true;});
  }

  int exitStatus;
  SyscallCall redirection;
  std::condition_variable exited;
  std::mutex exitLock;
};

class SandboxTest : public CppUnit::TestFixture {
  CPPUNIT_TEST_SUITE (SandboxTest);
  CPPUNIT_TEST (testSimpleProgram);
  CPPUNIT_TEST (testExitStatus);
  CPPUNIT_TEST_SUITE_END ();

  private:
  std::unique_ptr<TestSandbox> sbox;

  public:
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
      char* argv[3];
      argv[0] = strdup ("./build/Debug/syscall-tester");
      argv[1] = (char*)calloc (sizeof (char), 15);
      sprintf (argv[1], "%d", syscall);
      argv[2] = nullptr;
      sbox->spawn (argv);
      for (size_t i = 0; argv[i]; i++)
        free (argv[i]);
    }

    void testSimpleProgram()
    {
      _run (SYS_read);
      sbox->waitExit();
      CPPUNIT_ASSERT_EQUAL (0, sbox->exitStatus);
    }

    void testExitStatus()
    {
      _run (SYS_open);
      sbox->waitExit();
      CPPUNIT_ASSERT_EQUAL (14, sbox->exitStatus);
    }

    void testInterceptSyscall()
    {
      sbox->redirection.id = SYS_open;
      _run (SYS_read);
      sbox->waitExit();
      CPPUNIT_ASSERT_EQUAL (14, sbox->exitStatus);
    }

};


CPPUNIT_TEST_SUITE_REGISTRATION (SandboxTest);
