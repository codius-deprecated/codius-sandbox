#ifndef DIRENT_BUILDER_H
#define DIRENT_BUILDER_H

#include <dirent.h>
#include <vector>
#include <string>

struct linux_dirent {
  unsigned long d_ino;
  unsigned long d_off;
  unsigned short d_reclen;
  char d_name[];
  /*
   * char d_type;
   */
};

class DirentBuilder {
public:
  enum DirentType {
    Unknown = DT_UNKNOWN,
    FIFO = DT_FIFO,
    Character = DT_CHR,
    Directory = DT_DIR,
    Block = DT_BLK,
    Regular = DT_REG,
    Link = DT_LNK,
    Socket = DT_SOCK
  };

  DirentBuilder(int startIdx = 1);
  void append(const std::string& name, DirentType type = Regular);
  std::vector<char> data() const;

private:
  std::vector<char> m_buf;
  int m_idx;
  int m_inode;
};

#endif // DIRENT_BUILDER_H
