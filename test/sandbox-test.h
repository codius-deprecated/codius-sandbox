#include "thread-sandbox.h"
#include <cppunit/extensions/HelperMacros.h>

class TestSandbox : public ThreadSandbox {
public:
  TestSandbox();
  ~TestSandbox() {}

  SyscallCall handleSyscall(const SyscallCall& call) override;
  void handleSignal(int signal) override;
  void handleExit(int status) override;
  void waitExit();

  int exitStatus;
  std::vector<SyscallCall> history;
  std::map<SyscallCall, SyscallCall> remap;
};

class SandboxTest : public CppUnit::TestFixture {
public:
  virtual std::unique_ptr<TestSandbox> makeSandbox();
  void setUp();
  void tearDown();

  std::unique_ptr<TestSandbox> sbox;
};
