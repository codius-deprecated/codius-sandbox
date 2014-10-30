#include "vfs.h"
#include <dirent.h>
#include <memory.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

struct linux_dirent {
  unsigned long d_ino;
  unsigned long d_off;
  unsigned short d_reclen;
  char d_name[];
  /*
  char pad
  char d_type;
  */
};

class DirentBuilder {
public:
  void append(const std::string& name) {
    m_names.push_back (name);
  }

  std::vector<char> data() const {
    std::vector<char> ret;
    for (auto i = m_names.cbegin(); i != m_names.cend(); i++) {
      push (ret, *i);
    }
    return ret;
  }

private:
  std::vector<std::string> m_names;

  void push(std::vector<char>& ret, const std::string& name) const {
    static long inode = 4242;
    unsigned short reclen;
    char* d_type;

    reclen = sizeof (linux_dirent) + name.size() + sizeof (char) * 3;
    off_t start = ret.size();
    ret.resize (start + reclen);
    linux_dirent* ent = reinterpret_cast<linux_dirent*>(&ret.data()[start]);
    ent->d_ino = inode++;
    ent->d_reclen = reclen;
    memcpy (ent->d_name, name.data(), name.size() + 1);
    d_type = reinterpret_cast<char*>(ent + reclen - 1);
    *d_type = DT_REG;
  }
};

VFS::VFS(Sandbox* sandbox)
  : m_sbox (sandbox)
  , m_fs (new NativeFilesystem("/tmp/sbox-root/"))
{
  m_whitelist.push_back ("/lib64/tls/x86_64/libc.so.6");
  m_whitelist.push_back ("/lib64/tls/libc.so.6");
  m_whitelist.push_back ("/lib64/x86_64/libc.so.6");
  m_whitelist.push_back ("/lib64/libc.so.6");
  m_whitelist.push_back ("/etc/ld.so.cache");
}

std::string
VFS::getFilename(Sandbox::Address addr) const
{
  std::vector<char> buf (1024);
  m_sbox->copyString (addr, buf.size(), buf.data());
  return std::string (buf.data());
}


int
VFS::fromVirtualFD(int fd)
{
  if (fd >= 4095)
    return fd - 4095;
  return -1;
}

int
VFS::toVirtualFD(int fd)
{
  return fd + 4095;
}

void
VFS::do_openat (Sandbox::SyscallCall& call)
{
  int fd = fromVirtualFD (call.args[0]);
  std::string fname = getFilename (call.args[1]);
  if (call.args[0] == AT_FDCWD) {
    std::string cwd = m_sbox->getCWD();
    std::string absPath;

    if (fname[0] == '/')
      absPath = fname;
    else
      absPath = cwd + absPath;

    if (!isWhitelisted (fname)) {
      call.id = -1;
      call.returnVal = toVirtualFD (m_fs->open (absPath.c_str(), call.args[2]));
    }
  } else {
    //FIXME: fd should be verified to make sure one can't do dup2(AT_FDCWD, foo)
    call.id = -1;
    call.returnVal = m_fs->openat (fd, fname.c_str(), call.args[2], call.args[3]);
  }
}

void
VFS::do_open (Sandbox::SyscallCall& call)
{
  std::string fname = getFilename (call.args[0]);
  if (!isWhitelisted (fname)) {
    call.id = -1;
    call.returnVal = toVirtualFD (m_fs->open (fname.c_str(), call.args[1]));
  }
}

void
VFS::do_close (Sandbox::SyscallCall& call)
{
  int fh = fromVirtualFD (call.args[0]);
  if (fh > 0) {
    call.id = -1;
    call.returnVal = m_fs->close (fh);
  }
}

void
VFS::do_read (Sandbox::SyscallCall& call)
{
  int fh = fromVirtualFD (call.args[0]);
  if (fh > 0) {
    call.id = -1;
    std::vector<char> buf (call.args[2]);
    ssize_t readCount = m_fs->read (fh, buf.data(), buf.size());
    m_sbox->writeData (call.args[1], buf.size(), buf.data());
    call.returnVal = readCount;
  }
}

void
VFS::do_fstat (Sandbox::SyscallCall& call)
{
  int fd = fromVirtualFD (call.args[0]);
  if (fd > -1) {
    call.id = -1;
    struct stat sbuf;
    call.returnVal = m_fs->fstat (fd, &sbuf);
    m_sbox->writeData(call.args[1], sizeof (sbuf), (char*)&sbuf);
  }
}

void
VFS::do_getdents (Sandbox::SyscallCall& call)
{
  int fd = fromVirtualFD (call.args[0]);
  if (fd > -1) {
    void* buf = malloc (call.args[2]);
    call.id = -1;
    call.returnVal = m_fs->getdents (fd, (struct linux_dirent*)buf, call.args[2]);
    m_sbox->writeData(call.args[1], call.args[2], (char*)buf);
    free (buf);
  }
}

#define HANDLE_CALL(x) case SYS_##x: do_##x(ret);break;

Sandbox::SyscallCall
VFS::handleSyscall(const Sandbox::SyscallCall& call)
{
  Sandbox::SyscallCall ret(call);
  switch (call.id) {
    HANDLE_CALL (open);
    HANDLE_CALL (close);
    HANDLE_CALL (read);
    HANDLE_CALL (fstat);
    HANDLE_CALL (getdents);
    HANDLE_CALL (openat);
  }
  return ret;
}

bool
VFS::isWhitelisted(const std::string& str)
{
  for (auto i = m_whitelist.cbegin(); i != m_whitelist.cend(); i++) {
    if (str == *i)
      return true;
  }
  return false;
}

int
NativeFilesystem::open(const char* name, int flags)
{
  std::string newName (name);
  newName = m_root + "/" + newName;
  return ::open (newName.c_str(), flags);
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
  /*DirentBuilder builder;
  static bool read = false;
  if (!read) {
    std::vector<char> buf;
    builder.append ("hello");
    builder.append ("codius");
    buf = builder.data();
    read = true;
    memcpy (dirs, buf.data(), count);
    return buf.size();
  }
  return 0;*/
}

int
NativeFilesystem::openat(int fd, const char* filename, int flags, mode_t mode)
{
  return ::openat (fd, filename, flags, mode);
}

NativeFilesystem::NativeFilesystem(const std::string& root)
  : Filesystem()
  , m_root (root)
{
}

Filesystem::Filesystem()
{}
