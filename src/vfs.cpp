#include "vfs.h"
#include <dirent.h>
#include <memory.h>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <cassert>
#include <error.h>
#include <fcntl.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
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

File::Ptr
VFS::getFile(int fd) const
{
  assert (isVirtualFD (fd));
  return m_openFiles.at(fd);
}

int
File::close()
{
  if (m_localFD > 0) {
    int ret = m_fs->close(m_localFD);
    m_localFD = -1;
    return ret;
  }
  return -EBADF;
}

std::pair<std::string, std::shared_ptr<Filesystem> >
VFS::getFilesystem(const std::string& path) const
{
  std::string searchPath (path);
  std::string longest_mount;
  if (path[0] == '.')
    searchPath = m_cwd->path() + path;
  for(auto i = m_mountpoints.cbegin(); i != m_mountpoints.cend(); i++) {
    if (searchPath.compare(0, i->first.size(), i->first) == 0) {
      std::string newPath (searchPath.substr (i->first.size()-1));
      return std::make_pair (newPath, i->second);
    }
  }
  return std::make_pair (std::string(), nullptr);
}

int File::s_nextFD = VFS::firstVirtualFD;

File::File(int localFD, const std::string& path, std::shared_ptr<Filesystem>& fs)
  : m_localFD (localFD),
    m_path (path),
    m_fs (fs)
{
  //FIXME: gcc-4.8 lacks stdatomic.h, so we're stuck with gcc builtins :(
  //see also: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=58016
  m_virtualFD = __sync_fetch_and_add (&s_nextFD, 1);
}

std::string
File::path() const
{
  return m_path;
}

void
VFS::do_openat (Sandbox::SyscallCall& call)
{
  std::string fname = getFilename (call.args[1]);

  if (fname[0] != '/') {
    std::string fdPath;
    if (call.args[0] == AT_FDCWD) {
      fdPath = m_cwd->path();
    } else if (isVirtualFD (call.args[0])) {
      File::Ptr file = getFile (call.args[0]);
      fdPath = file->path();
    }
    fname = fdPath + fname;
  }

  openFile (call, fname);
}


File::~File ()
{
  close();
}

File::Ptr
VFS::makeFile (int fd, const std::string& path, std::shared_ptr<Filesystem>& fs)
{
  File::Ptr f(new File (fd, path, fs));
  m_openFiles.insert (std::make_pair (f->virtualFD(), f));
  return f;
}

void
VFS::do_access (Sandbox::SyscallCall& call)
{
  std::string fname = getFilename (call.args[0]);
  if (!isWhitelisted (fname)) {
    call.id = -1;
    std::pair<std::string, std::shared_ptr<Filesystem> > fs = getFilesystem (fname);
    if (fs.second) {
      call.returnVal = fs.second->access (fs.first.c_str(), call.args[1]);
    } else {
      call.returnVal = -ENOENT;
    }
  }
}

void
VFS::openFile(Sandbox::SyscallCall& call, const std::string& fname)
{
  if (!isWhitelisted (fname)) {
    call.id = -1;
    std::pair<std::string, std::shared_ptr<Filesystem> > fs = getFilesystem (fname);
    if (fs.second) {
      int fd = fs.second->open (fs.first.c_str(), call.args[1], call.args[2]);
      File::Ptr file (makeFile (fd, fname, fs.second));
      call.returnVal = file->virtualFD();
    } else {
      call.returnVal = -ENOENT;
    }
  }
}

void
VFS::do_open (Sandbox::SyscallCall& call)
{
  std::string fname = getFilename (call.args[0]);
  openFile (call, fname);
}

int
File::virtualFD() const
{
  return m_virtualFD;
}

int
File::localFD() const
{
  return m_localFD;
}

std::shared_ptr<Filesystem>
File::fs() const
{
  return m_fs;
}

void
VFS::do_close (Sandbox::SyscallCall& call)
{
  if (isVirtualFD (call.args[0])) {
    call.id = -1;
    File::Ptr fh = getFile (call.args[0]);
    if (fh) {
      call.returnVal = fh->close ();
      m_openFiles.erase (fh->virtualFD());
    } else {
      call.returnVal = -EBADF;
    }
  }
}

ssize_t
File::read(void* buf, size_t count)
{
  return m_fs->read (m_localFD, buf, count);
}

void
VFS::do_read (Sandbox::SyscallCall& call)
{
  if (isVirtualFD (call.args[0])) {
    call.id = -1;
    File::Ptr file = getFile (call.args[0]);
    std::vector<char> buf (call.args[2]);
    if (file) {
      ssize_t readCount = file->read (buf.data(), buf.size());
      if (readCount >= 0) {
        m_sbox->writeData (call.args[1], readCount, buf.data());
        call.returnVal = readCount;
      } else {
        call.returnVal = -errno;
      }
    } else {
      call.returnVal = -EBADF;
    }
  }
}

int
File::fstat (struct stat* buf)
{
  return m_fs->fstat (m_localFD, buf);
}

void
VFS::do_fstat (Sandbox::SyscallCall& call)
{
  if (isVirtualFD (call.args[0])) {
    File::Ptr file = getFile (call.args[0]);
    call.id = -1;
    if (file) {
      struct stat sbuf;
      call.returnVal = file->fstat (&sbuf);
      if (call.returnVal == 0)
        m_sbox->writeData(call.args[1], sizeof (sbuf), (char*)&sbuf);
    } else {
      call.returnVal = -EBADF;
    }
  }
}

int
File::getdents(struct linux_dirent* dirs, unsigned int count)
{
  return m_fs->getdents (m_localFD, dirs, count);
}

void
VFS::do_write (Sandbox::SyscallCall& call)
{
  if (isVirtualFD (call.args[0])) {
    File::Ptr file = getFile (call.args[0]);
    call.id = -1;
    if (file) {
      std::vector<char> buf (call.args[2]);
      m_sbox->copyData (call.args[1], buf.size(), buf.data());
      call.returnVal = file->write (buf.data(), buf.size());
    }
  }
}

void
VFS::do_getdents (Sandbox::SyscallCall& call)
{
  if (isVirtualFD (call.args[0])) {
    File::Ptr file = getFile (call.args[0]);
    call.id = -1;
    if (file) {
      std::vector<char> buf (call.args[2]);
      struct linux_dirent* dirents = (struct linux_dirent*)buf.data();
      call.returnVal = file->getdents (dirents, buf.size());
      if ((int)call.returnVal > 0)
        m_sbox->writeData(call.args[1], call.returnVal, buf.data());
    } else {
      call.returnVal = -EBADF;
    }
  }
}

void
VFS::do_fchdir(Sandbox::SyscallCall& call)
{
  File::Ptr fh = getFile (call.args[0]);
  if (fh) {
    m_cwd = fh;
    call.returnVal = 0;
  } else {
    call.returnVal = -EBADF;
  }
}

void
VFS::do_chdir(Sandbox::SyscallCall& call)
{
  std::string fname = getFilename (call.args[0]);
  call.returnVal = setCWD (fname);
}

std::string
VFS::getCWD() const
{
  assert (m_cwd);
  return m_cwd->path();
}

int
VFS::setCWD(const std::string& fname)
{
  std::pair<std::string, std::shared_ptr<Filesystem> > fs = getFilesystem (fname);
  if (fs.second) {
    int fd = fs.second->open (fs.first.c_str(), O_DIRECTORY, 0);
    m_cwd = File::Ptr (new File (fd, fname, fs.second));
    return 0;
  } else {
    return -ENOENT;
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
    HANDLE_CALL (lseek);
    HANDLE_CALL (write);
    HANDLE_CALL (access);
    HANDLE_CALL (chdir);
    HANDLE_CALL (stat);
    HANDLE_CALL (lstat);
  }
  return ret;
}

#undef HANDLE_CALL

off_t
File::lseek(off_t offset, int whence)
{
  return m_fs->lseek(m_localFD, offset, whence);
}

ssize_t
File::write(void* buf, size_t count)
{
  return m_fs->write (m_localFD, buf, count);
}

void
VFS::do_lstat(Sandbox::SyscallCall& call)
{
  std::string fname = getFilename (call.args[0]);
  if (!isWhitelisted (fname)) {
    call.id = -1;
    std::pair<std::string, std::shared_ptr<Filesystem> > fs = getFilesystem (fname);
    if (fs.second) {
      struct stat sbuf;
      call.returnVal = fs.second->lstat (fname.c_str(), &sbuf);
      if (call.returnVal == 0)
        m_sbox->writeData (call.args[1], sizeof (sbuf), (char*)&sbuf);
    } else {
      call.returnVal = -ENOENT;
    }
  }
}

void
VFS::do_stat(Sandbox::SyscallCall& call)
{
  std::string fname = getFilename (call.args[0]);
  if (!isWhitelisted (fname)) {
    call.id = -1;
    std::pair<std::string, std::shared_ptr<Filesystem> > fs = getFilesystem (fname);
    if (fs.second) {
      struct stat sbuf;
      call.returnVal = fs.second->stat (fname.c_str(), &sbuf);
      if (call.returnVal == 0)
        m_sbox->writeData (call.args[1], sizeof (sbuf), (char*)&sbuf);
    } else {
      call.returnVal = -ENOENT;
    }
  }
}

void
VFS::do_lseek(Sandbox::SyscallCall& call)
{
  if (isVirtualFD (call.args[0])) {
    call.id = -1;
    File::Ptr file = getFile (call.args[0]);
    if (file) {
      call.returnVal = file->lseek (call.args[1], call.args[2]);
    } else {
      call.returnVal = -EBADF;
    }
  }
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

