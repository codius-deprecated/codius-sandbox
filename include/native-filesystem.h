#ifndef NATIVE_FILESYSTEM_H
#define NATIVE_FILESYSTEM_H

#include "filesystem.h"
#include <string>
#include <map>

/**
 * A filesystem that directly interacts with the host's local filesystem
 */
class NativeFilesystem : public Filesystem {
public:
  /**
   * Constructor. Accepts a path that is the root of this filesystem
   *
   * @param root Root of this filesystem
   */
  NativeFilesystem(const std::string& root);
  virtual int open(const char* name, int flags, int mode);
  virtual ssize_t read(int fd, void* buf, size_t count);
  virtual int close(int fd);
  virtual int fstat(int fd, struct stat* buf);
  virtual int getdents(int fd, struct linux_dirent* dirs, unsigned int count);
  virtual off_t lseek(int fd, off_t offset, int whence);
  virtual ssize_t write(int fd, void* buf, size_t count);
  virtual int access(const char* name, int mode);

private:
  std::string m_root;
  std::map<int, std::string> m_openFiles;
};

#endif // NATIVE_FILESYSTEM_H
