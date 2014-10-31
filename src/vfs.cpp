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
#include <cassert>
#include <error.h>
#include <asm-generic/posix_types.h>
#include "dirent-builder.h"

VFS::VFS(Sandbox* sandbox)
  : m_sbox (sandbox)
{
  m_whitelist.push_back ("/lib64/tls/x86_64/libc.so.6");
  m_whitelist.push_back ("/lib64/tls/libc.so.6");
  m_whitelist.push_back ("/lib64/x86_64/libc.so.6");
  m_whitelist.push_back ("/lib64/libc.so.6");
  m_whitelist.push_back ("/etc/ld.so.cache");
  //mountFilesystem (std::string("/codius"), std::shared_ptr<Filesystem>(new NativeFilesystem ("/tmp/sbox-root")));
}

void
VFS::mountFilesystem(const std::string& path, std::shared_ptr<Filesystem> fs)
{
  m_mountpoints.insert (std::make_pair (path, fs));
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

std::shared_ptr<Filesystem>
VFS::getFilesystem(int fd) const
{
  return m_openFiles.at(fd);
}

std::pair<std::string, std::shared_ptr<Filesystem> >
VFS::getFilesystem(const std::string& path) const
{
  std::string longest_mount;
  for(auto i = m_mountpoints.cbegin(); i != m_mountpoints.cend(); i++) {
    if (path.compare(0, i->first.size(), i->first) == 0) {
      std::string newPath (path.substr (i->first.size()));
      return std::make_pair (newPath, i->second);
    }
  }
  return std::make_pair (std::string(), nullptr);
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
      std::pair<std::string, std::shared_ptr<Filesystem> > fs = getFilesystem (fname);
      if (fs.second) {
        int fd = fs.second->open (fs.first.c_str(), call.args[2]);
        m_openFiles.insert (std::make_pair (fd, fs.second));
        call.returnVal = toVirtualFD (fd);
      } else {
        call.returnVal = -ENOENT;
      }
    }
  } else {
    //FIXME: fd should be verified to make sure one can't do dup2(AT_FDCWD, foo)
    //FIXME: Should also use map of opened filenames to figure out what
    //filesystem we end up on
    call.id = -1;
    std::pair<std::string, std::shared_ptr<Filesystem> > fs = getFilesystem (fname);
    if (fs.second) {
      call.returnVal = fs.second->openat (fd, fs.first.c_str(), call.args[2], call.args[3]);
    } else {
      call.returnVal = -ENOENT;
    }
  }
}

void
VFS::do_open (Sandbox::SyscallCall& call)
{
  std::string fname = getFilename (call.args[0]);
  if (!isWhitelisted (fname)) {
    call.id = -1;
    std::pair<std::string, std::shared_ptr<Filesystem> > fs = getFilesystem (fname);
    if (fs.second) {
      int fd = fs.second->open (fs.first.c_str(), call.args[0]);
      m_openFiles.insert (std::make_pair (fd, fs.second));
      call.returnVal = toVirtualFD (fd);
    } else {
      call.returnVal = -ENOENT;
    }
  }
}

void
VFS::do_close (Sandbox::SyscallCall& call)
{
  int fh = fromVirtualFD (call.args[0]);
  if (fh > 0) {
    call.id = -1;
    std::shared_ptr<Filesystem> fs = getFilesystem (fh);
    if (fs) {
      call.returnVal = fs->close (fh);
    } else {
      call.returnVal = -EBADF;
    }
  }
}

void
VFS::do_read (Sandbox::SyscallCall& call)
{
  int fh = fromVirtualFD (call.args[0]);
  if (fh > 0) {
    call.id = -1;
    std::vector<char> buf (call.args[2]);
    std::shared_ptr<Filesystem> fs = getFilesystem (fh);
    if (fs) {
      ssize_t readCount = fs->read (fh, buf.data(), buf.size());
      m_sbox->writeData (call.args[1], buf.size(), buf.data());
      call.returnVal = readCount;
    } else {
      call.returnVal = -EBADF;
    }
  }
}

void
VFS::do_fstat (Sandbox::SyscallCall& call)
{
  int fd = fromVirtualFD (call.args[0]);
  if (fd > -1) {
    struct stat sbuf;
    call.id = -1;
    std::shared_ptr<Filesystem> fs = getFilesystem (fd);
    if (fs) {
      call.returnVal = fs->fstat (fd, &sbuf);
      m_sbox->writeData(call.args[1], sizeof (sbuf), (char*)&sbuf);
    } else {
      call.returnVal = -EBADF;
    }
  }
}

void
VFS::do_getdents (Sandbox::SyscallCall& call)
{
  int fd = fromVirtualFD (call.args[0]);
  if (fd > -1) {
    call.id = -1;
    std::shared_ptr<Filesystem> fs = getFilesystem (fd);
    if (fs) {
      void* buf = malloc (call.args[2]);
      call.returnVal = fs->getdents (fd, (struct linux_dirent*)buf, call.args[2]);
      m_sbox->writeData(call.args[1], call.args[2], (char*)buf);
      free (buf);
    } else {
      call.returnVal = -EBADF;
    }
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
