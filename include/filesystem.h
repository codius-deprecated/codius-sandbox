#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include <unistd.h>

/**
 * Interface for implementing concrete filesystems
 *
 * Each function is an implementation of a specific POSIX syscall.
 *
 * @see VFS
 * @see Syscall manpages
 */
class Filesystem {
public:
  virtual int open(const char* name, int flags, int mode) = 0;
  virtual ssize_t read(int fd, void* buf, size_t count) = 0;
  virtual int close(int fd) = 0;
  virtual int fstat(int fd, struct stat* buf) = 0;
  virtual int getdents(int fd, struct linux_dirent* dirs, unsigned int count) = 0;
  virtual off_t lseek(int fd, off_t offset, int whence) = 0;
  virtual ssize_t write(int fd, void* buf, size_t count) = 0;
  virtual int access(const char* name, int mode) = 0;
};

#endif // FILESYSTEM_H
