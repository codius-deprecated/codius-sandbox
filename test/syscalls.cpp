#include "sandbox-test.h"
#include <unistd.h>
#include <sys/syscall.h>

static void
run_syscall(void* data)
{
  Sandbox::SyscallCall call = *static_cast<Sandbox::SyscallCall*>(data);
  syscall (call.id, call.args[0], call.args[1], call.args[2], call.args[3], call.args[4], call.args[5], call.args[6]);
  exit (errno);
}

class SyscallsTest : public SandboxTest {
  CPPUNIT_TEST_SUITE (SyscallsTest);
  CPPUNIT_TEST (testSimpleProgram);
  CPPUNIT_TEST (testExitStatus);
  CPPUNIT_TEST (testBadSyscall);
  CPPUNIT_TEST_SUITE_END ();

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

    using Word = Sandbox::Word;

    void _run (Word syscall, Word arg0 = 0, Word arg1 = 0, Word arg2 = 0, Word arg3 = 0, Word arg4 = 0, Word arg5 = 0)
    {
      Sandbox::SyscallCall args;
      args.id = syscall;
      args.args[0] = arg0;
      args.args[1] = arg1;
      args.args[2] = arg2;
      args.args[3] = arg3;
      args.args[4] = arg4;
      args.args[5] = arg5;
      sbox->spawn (run_syscall, &args);
    }

    void testSimpleProgram()
    {
      _run (SYS_exit);
      sbox->waitExit();
      CPPUNIT_ASSERT_EQUAL (0, sbox->exitStatus);
    }

    void testExitStatus()
    {
      _run (SYS_exit, 255);
      sbox->waitExit();
      CPPUNIT_ASSERT_EQUAL (255, sbox->exitStatus);
    }

    void testBadSyscall()
    {
      _run (SYS_vfork);
      sbox->waitExit();
      CPPUNIT_ASSERT_EQUAL (SIGSYS, sbox->exitStatus);
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

CPPUNIT_TEST_SUITE_REGISTRATION (SyscallsTest);
