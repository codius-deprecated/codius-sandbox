#ifndef CODIUS_VIRTUALFD_H
#define CODIUS_VIRTUALFD_H

#include "sandbox.h"
#include <memory>
#include <vector>

class VirtualFD {
public:
  using Ptr = std::shared_ptr<VirtualFD>;
  VirtualFD(int localFD);
  int localFD() const;
  int virtualFD() const;

  /**
   * Determines if a given file descriptor number is within the range of virtual
   * file descriptors
   *
   * @param fd File descriptor number
   * @return True if @p fd is within the range of virtual file descriptors,
   * false otherwise.
   */
  static inline bool isVirtualFD (int fd) {return fd >= firstVirtualFD;}

  /**
   * Start of the virtual file descriptor range
   */
  static constexpr int firstVirtualFD = 4096;

protected:
  void invalidate();

private:
  static int s_nextFD;
  int m_localFD;
  int m_virtualFD;
};

class VirtualFDGenerator {
public:
  bool isVirtualFD (int fd) const;
  VirtualFD::Ptr getVirtualFD (int fd) const;
  virtual Sandbox::SyscallCall handleSyscall (const Sandbox::SyscallCall& call) = 0;

protected:
  void registerFD(VirtualFD::Ptr fd);
  void removeFD(VirtualFD::Ptr fd);

private:
  std::map<int, VirtualFD::Ptr> m_openFDs;
};


#endif // CODIUS_VIRTUALFD_H
