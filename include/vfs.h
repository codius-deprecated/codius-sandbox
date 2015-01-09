#ifndef VFS_H
#define VFS_H

#include "sandbox.h"

class Filesystem;

class File {
public:
  File(int localFD, const std::string& path, std::shared_ptr<Filesystem>& fs);
  ~File();

  using Ptr = std::shared_ptr<File>;

  int localFD() const;
  int virtualFD() const;
  std::shared_ptr<Filesystem> fs() const;

  int close();
  int fstat(struct stat* buf);
  int getdents(struct linux_dirent* dirs, unsigned int count);
  ssize_t read(void* buf, size_t count);
  off_t lseek(off_t offset, int whence);
  ssize_t write(void* buf, size_t count);

  std::string path() const;

private:
  static int s_nextFD;
  int m_localFD;
  int m_virtualFD;
  std::string m_path;
  std::shared_ptr<Filesystem> m_fs;
};

/**
 * Implementation of virtual filesystem layer inside a sandbox
 */
class VFS {
public:
  /**
   * Constructor
   *
   * @param sandbox Sandbox this VFS is attached to
   */
  VFS();

  /**
   * Handles filesystem related syscalls
   */
  Sandbox::SyscallCall handleSyscall(const Sandbox::SyscallCall& call);

  /**
   * Copies a string out of a sandboxed process' memory
   *
   * @see Sandbox::copyString()
   */
  std::string getFilename(pid_t pid, Sandbox::Address addr) const;

  /**
   * Get the filesystem and filesystem-specific path for a given path
   *
   * @param path Path to use
   * @return A pair of (filesystem-local path, Filesystem object). If no such
   * filesystem is mounted for a given path, the Filesystem will be null
   */
  std::pair<std::string, std::shared_ptr<Filesystem> > getFilesystem(const std::string& path) const;

  /**
   * Get a previously-opened file from a virtual file descriptor
   *
   * @param fd Virtual file descriptor
   * @return A previously opened File, or null pointer
   */
  File::Ptr getFile(int fd) const;

  /**
   * Determines if a given file descriptor number is within the range of virtual
   * file descriptors
   *
   * @param fd File descriptor number
   * @return True if @p fd is within the range of virtual file descriptors,
   * false otherwise.
   */
  inline bool isVirtualFD (int fd) const {return fd >= firstVirtualFD;}

  /**
   * Start of the virtual file descriptor range
   */
  static constexpr int firstVirtualFD = 4096;

  /**
   * Mount a Filesystem onto a given path
   */
  void mountFilesystem(const std::string& path, std::shared_ptr<Filesystem> fs);

  /**
   * Get the path of the current directory for this VFS
   *
   * @return Path of the current directory
   */
  std::string getCWD() const;

  /**
   * Set the current directory used for local path resolution. The underlying
   * filesystem must support open() with O_DIRECTORY.
   *
   * @param path Path to set new cwd to
   * @return 0 on success, negative error number otherwise.
   */
  int setCWD(const std::string& path);

private:
  std::map<std::string, std::shared_ptr <Filesystem>> m_mountpoints;
  std::map<int, File::Ptr> m_openFiles;
  std::vector<std::string> m_whitelist;
  File::Ptr m_cwd;

  bool isWhitelisted(const std::string& str);

  void openFile(Sandbox::SyscallCall& call, const std::string& fname, int flags, mode_t mode);

  void do_open(Sandbox::SyscallCall& call);
  void do_close(Sandbox::SyscallCall& call);
  void do_read(Sandbox::SyscallCall& call);
  void do_fstat(Sandbox::SyscallCall& call);
  void do_getdents(Sandbox::SyscallCall& call);
  void do_openat(Sandbox::SyscallCall& call);
  void do_lseek(Sandbox::SyscallCall& call);
  void do_write(Sandbox::SyscallCall& call);
  void do_access(Sandbox::SyscallCall& call);
  void do_chdir(Sandbox::SyscallCall& call);
  void do_fchdir(Sandbox::SyscallCall& call);
  void do_stat(Sandbox::SyscallCall& call);
  void do_lstat(Sandbox::SyscallCall& call);
  void do_getcwd(Sandbox::SyscallCall& call);
  void do_readlink(Sandbox::SyscallCall& call);

  File::Ptr makeFile (int fd, const std::string& path, std::shared_ptr<Filesystem>& fs);
};

#endif // VFS_H
