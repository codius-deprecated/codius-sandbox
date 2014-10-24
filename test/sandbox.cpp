#include <cppunit/extensions/HelperMacros.h>
#include "sandbox.h"
#include <string.h>
#include <condition_variable>
#include <mutex>
#include <uv.h>

class TestSandbox : public Sandbox {
public:
  TestSandbox() : Sandbox(),
                  exitStatus(-1) {
  }
  SyscallCall handleSyscall(const SyscallCall& call) override {return call;}
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

    void testSimpleProgram()
    {
      static char* argv[2];
      argv[0] = strdup ("/usr/bin/true");
      argv[1] = nullptr;
      sbox->spawn (argv);
      sbox->waitExit();
      CPPUNIT_ASSERT_EQUAL (0, sbox->exitStatus);
    }

    void testExitStatus()
    {
      static char* argv[2];
      argv[0] = strdup ("/usr/bin/false");
      argv[1] = nullptr;
      sbox->spawn (argv);
      sbox->waitExit();
      CPPUNIT_ASSERT_EQUAL (1, sbox->exitStatus);
    }

};


CPPUNIT_TEST_SUITE_REGISTRATION (SandboxTest);
