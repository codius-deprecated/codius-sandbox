#include "vfs.h"
#include <dirent.h>
#include <memory.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <iostream>
#include <stdio.h>

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
{
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

Sandbox::SyscallCall
VFS::handleSyscall(const Sandbox::SyscallCall& call)
{
  Sandbox::SyscallCall ret(call);
  if (call.id == SYS_open) {
    std::string fname = getFilename(call.args[0]);
    std::cout << "attempt " << fname << std::endl;
    if (strncmp (fname.c_str(), "/usr/lib", strlen("/usr/lib")) != 0 && strncmp (fname.c_str(), "/lib", strlen("/lib") != 0)) {
      ret.id = -1;
      int fh = open (fname.c_str(), call.args[1]);
      if (fh > -1) {
        ret.returnVal = toVirtualFD(fh);
      } else {
        ret.returnVal = fh;
      }
      std::cout << "open " << fname << " = " << fh << std::endl;
    }
  } else if (call.id == __NR_close) {
    int fd = fromVirtualFD(call.args[0]);
    if (fd > -1) {
      ret.id = -1;
      ret.returnVal = close (fd);
      std::cout << "close " << fd << std::endl;
    }
  } else if (call.id == __NR_read) {
    int fd = fromVirtualFD(call.args[0]);
    if (fd > -1) {
      ret.id = -1;
      std::vector<char> buf (call.args[2]);
      ssize_t readCount = read (fd, buf.data(), buf.size());
      m_sbox->writeData (call.args[1], buf.size(), buf.data());
      ret.returnVal = readCount;
      std::cout << "read " << fd << " = " << readCount << std::endl;
      std::cout << std::hex;
      for (unsigned int i = 0;i < buf.size(); i++) {
        std::cout << (Sandbox::Word) buf[i] << " ";
      }
      std::cout << std::endl;
    }
  } else if (call.id == __NR_fstat) {
    int fd = fromVirtualFD(call.args[0]);
    if (fd > -1) {
      ret.id = -1;
      struct stat sbuf;
      ret.returnVal = fstat (fd, &sbuf);
      m_sbox->writeData(call.args[1], sizeof (sbuf), (char*)&sbuf);
    }
  } else if (call.id == __NR_getdents) {
    static bool read = false;
    ret.id = -1;
    std::cout << "call getdents" << std::endl;
    if (!read) {
      std::vector<char> buf;
      DirentBuilder builder;
      builder.append ("hello");
      builder.append ("codius");
      buf = builder.data();
      m_sbox->writeData(call.args[1], buf.size(), buf.data());
      ret.returnVal = buf.size();
      read = true;
    } else {
      ret.returnVal = 0;
    }
  }
  return ret;
}
