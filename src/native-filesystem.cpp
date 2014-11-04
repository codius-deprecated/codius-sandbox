#include <stdio.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "native-filesystem.h"

int
NativeFilesystem::open(const char* name, int flags, int mode)
{
  std::string newName (name);
  newName = m_root + "/" + newName;
  return ::open (newName.c_str(), flags, mode);
}

int
NativeFilesystem::close(int fd)
{
  return ::close (fd);
}

ssize_t
NativeFilesystem::read(int fd, void* buf, size_t count)
{
  return ::read (fd, buf, count);
}

int
NativeFilesystem::fstat(int fd, struct stat* buf)
{
  return ::fstat (fd, buf);
}

int
NativeFilesystem::getdents(int fd, struct linux_dirent* dirs, unsigned int count)
{
  return ::syscall (SYS_getdents, fd, dirs, count);
}

off_t
NativeFilesystem::lseek(int fd, off_t offset, int whence)
{
  return ::lseek (fd, offset, whence);
}

NativeFilesystem::NativeFilesystem(const std::string& root)
  : Filesystem()
  , m_root (root)
{
}

ssize_t
NativeFilesystem::write(int fd, void* buf, size_t count)
{
  return ::write (fd, buf, count);
}

int
NativeFilesystem::access(const char* name, int mode)
{
  return ::access (name, mode);
}

