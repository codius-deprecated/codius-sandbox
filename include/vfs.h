#ifndef VFS_H
#define VFS_H

#include "sandbox.h"
#include <memory>
#include <vector>

class Filesystem;
class DirentBuilder;

class Filesystem {
public:
  Filesystem();

  int open(const char* name, int flags);
  ssize_t read(int fd, void* buf, size_t count);
  int close(int fd);
  int fstat(int fd, struct stat* buf);
  void getdents(int fd, DirentBuilder& builder);
};

class VFS {
public:
  VFS(Sandbox* sandbox);

  Sandbox::SyscallCall handleSyscall(const Sandbox::SyscallCall& call);
  std::string getFilename(Sandbox::Address addr) const;

private:
  Sandbox* m_sbox;
  std::unique_ptr<Filesystem> m_fs;
  std::vector<std::string> m_whitelist;

  bool isWhitelisted(const std::string& str);
  int fromVirtualFD(int fd);
  int toVirtualFD(int fd);

  void do_open(Sandbox::SyscallCall& call);
  void do_close(Sandbox::SyscallCall& call);
  void do_read(Sandbox::SyscallCall& call);
  void do_fstat(Sandbox::SyscallCall& call);
  void do_getdents(Sandbox::SyscallCall& call);
};

#endif // VFS_H
