#ifndef VFS_H
#define VFS_H

#include "sandbox.h"
#include <memory>
#include <vector>

class VFS {
public:
  VFS(Sandbox* sandbox);

  Sandbox::SyscallCall handleSyscall(const Sandbox::SyscallCall& call);
  std::string getFilename(Sandbox::Address addr) const;

private:
  int fromVirtualFD(int fd);
  int toVirtualFD(int fd);
  Sandbox* m_sbox;
};

#endif // VFS_H
