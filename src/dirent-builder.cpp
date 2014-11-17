#include "dirent-builder.h"

#include <memory.h>

DirentBuilder::DirentBuilder(int startIdx)
  : m_idx (startIdx),
    m_inode (256)
{}

void
DirentBuilder::append(const std::string& name, DirentType type) {
  unsigned short reclen = 24;
  unsigned short min_reclen;

  min_reclen = sizeof (((linux_dirent*)0)->d_ino) + sizeof (((linux_dirent*)0)->d_off) + sizeof (((linux_dirent*)0)->d_reclen) + name.size() + 2;
  while (reclen < min_reclen) {
    reclen += 8;
  }
  off_t start = m_buf.size();
  m_buf.resize (start + reclen);
  memset (&m_buf.data()[start], 0, reclen);
  linux_dirent* ent = reinterpret_cast<linux_dirent*>(&m_buf.data()[start]);
  ent->d_ino = m_inode++;
  ent->d_off = m_idx++;
  ent->d_reclen = reclen;
  memcpy (ent->d_name, name.c_str(), name.size());
  m_buf.data()[start + min_reclen - 2] = 0;
  m_buf.data()[start + reclen - 1] = type;
}

std::vector<char>
DirentBuilder::data() const {
  return m_buf;
}

