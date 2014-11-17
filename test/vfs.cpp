#include "sandbox-test.h"

#include "filesystem.h"
#include "vfs.h"

#include <cppunit/extensions/HelperMacros.h>
#include <string.h>

#include <sys/types.h>
#include <fcntl.h>
#include <cassert>

class TestFS : public Filesystem {
  int open(const char* name, int flags, int mode)
  {
    if (strcmp (name, "null") == 0)
      return 1;
    return -ENOENT;
  }

  ssize_t read(int fd, void* buf, size_t count)
  {
    return -ENOSYS;
  }

  int close(int fd)
  {
    return -ENOSYS;
  }

  int fstat(int fd, struct stat* buf)
  {
    return -ENOSYS;
  }

  int getdents(int fd, struct linux_dirent* dirs, unsigned int count)
  {
    return -ENOSYS;
  }

  off_t lseek(int fd, off_t offset, int whence)
  {
    return -ENOSYS;
  }

  ssize_t write(int fd, void* buf, size_t count)
  {
    return -ENOSYS;
  }

  int access(const char* name, int mode)
  {
    return -ENOSYS;
  }

  int stat(const char* path, struct stat *buf)
  {
    return -ENOSYS;
  }

  int lstat(const char* path, struct stat *buf)
  {
    return -ENOSYS;
  }

  ssize_t readlink(const char* path, char* buf, size_t bufsize)
  {
    return -ENOSYS;
  }

};

class VFSTest : public SandboxTest {
  CPPUNIT_TEST_SUITE (VFSTest);
  CPPUNIT_TEST (testOpenFile);
  CPPUNIT_TEST (testOpenMissingFile);
  CPPUNIT_TEST_SUITE_END ();

public:
  struct runner {
    VFSTest* self;
    void (VFSTest::*func)(void*);
  };

  static void _runFunc(void* d) {
    runner r = *static_cast<runner*>(d);
    try {
      ((r.self)->*(r.func))(nullptr);
    } catch (CppUnit::Exception e) {
      char buf[2048];
      memset (buf, 0, sizeof (buf));
      sprintf (buf, "%s\t%s\t%d", e.what(), e.sourceLine().fileName().c_str(), e.sourceLine().lineNumber());
      write (42, buf, sizeof (buf));
    }
  }

  void _run(void (VFSTest::*func)(void*)) {
    runner r = {
      this,
      func
    };
    sbox->spawn(_runFunc, &r);
  }

  void sbox_testOpenFile(void*)
  {
    int fh = ::open("/test-fs/null", O_RDONLY);
    assert (fh);
    CPPUNIT_ASSERT_EQUAL (0, errno);
    close (fh);
  }

  void testOpenFile()
  {
    _run(&VFSTest::sbox_testOpenFile);
    sbox->waitExit();
    CPPUNIT_ASSERT_EQUAL (0, sbox->exitStatus);
  }

  void sbox_testOpenMissingFile(void*)
  {
    open("/test-fs/missing", O_RDONLY);
    CPPUNIT_ASSERT_EQUAL (ENOENT, errno);
  }

  void testOpenMissingFile()
  {
    _run(&VFSTest::sbox_testOpenMissingFile);
    sbox->waitExit();
    CPPUNIT_ASSERT_EQUAL (0, sbox->exitStatus);
  }

  std::unique_ptr<TestSandbox> makeSandbox() override
  {
    std::unique_ptr<TestSandbox> s (new TestSandbox());
    s->getVFS().mountFilesystem ("/test-fs", std::shared_ptr<Filesystem> (new TestFS()));
    return s;
  }

};

CPPUNIT_TEST_SUITE_REGISTRATION (VFSTest);
