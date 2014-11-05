#ifndef VFS_H
#define VFS_H

#include "dirent-builder.h"
#include "sandbox.h"
#include <memory>
#include <vector>

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

class Filesystem {
public:
  Filesystem();

  virtual int open(const char* name, int flags) = 0;
  virtual ssize_t read(int fd, void* buf, size_t count) = 0;
  virtual int close(int fd) = 0;
  virtual int fstat(int fd, struct stat* buf) = 0;
  virtual int getdents(int fd, struct linux_dirent* dirs, unsigned int count) = 0;
  virtual off_t lseek(int fd, off_t offset, int whence) = 0;
  virtual ssize_t write(int fd, void* buf, size_t count) = 0;
  virtual int access(const char* name, int mode) = 0;
};

class NativeFilesystem : public Filesystem {
public:
  NativeFilesystem(const std::string& root);
  virtual int open(const char* name, int flags);
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

class VFS {
public:
  VFS(Sandbox* sandbox);

  Sandbox::SyscallCall handleSyscall(const Sandbox::SyscallCall& call);
  std::string getFilename(Sandbox::Address addr) const;
  std::pair<std::string, std::shared_ptr<Filesystem> > getFilesystem(const std::string& path) const;
  File::Ptr getFile(int fd) const;

  inline bool isVirtualFD (int fd) const {return fd >= firstVirtualFD;}
  static constexpr int firstVirtualFD = 4096;

  void mountFilesystem(const std::string& path, std::shared_ptr<Filesystem> fs);
  std::string getMountedFilename(const std::string& path) const;

  std::string getCWD() const;
  int setCWD(const std::string& str);

private:
  Sandbox* m_sbox;
  std::map<std::string, std::shared_ptr <Filesystem>> m_mountpoints;
  std::map<int, File::Ptr> m_openFiles;
  std::vector<std::string> m_whitelist;
  File::Ptr m_cwd;

  bool isWhitelisted(const std::string& str);

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

  File::Ptr makeFile (int fd, const std::string& path, std::shared_ptr<Filesystem>& fs);
};

#endif // VFS_H
