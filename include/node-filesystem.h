#ifndef NODE_FILESYSTEM_H
#define NODE_FILESYSTEM_H

#include "vfs.h"

#include <node.h>

class NodeSandbox;

class CodiusNodeFilesystem : public Filesystem {
public:
  CodiusNodeFilesystem(NodeSandbox* sbox);

  struct VFSResult {
    int errnum;
    v8::Handle<v8::Value> result;
  };

  VFSResult doVFS(const std::string& name, v8::Handle<v8::Value> argv[], int argc);

  int open(const char* name, int flags, int mode) override;
  ssize_t read(int fd, void* buf, size_t count) override;
  int close (int fd) override;
  int fstat (int fd, struct stat* buf) override;
  int getdents (int fd, struct linux_dirent* dirs, unsigned int count) override;
  off_t lseek (int fd, off_t offset, int whence) override;
  ssize_t write(int fd, void* buf, size_t count) override;
  int access (const char* name, int mode) override;
  int stat (const char* name, struct stat* buf) override;
  int lstat (const char* name, struct stat* buf) override;
  ssize_t readlink(const char* path, char* buf, size_t bufsize);

private:
  NodeSandbox* m_sbox;
};

#endif // NODE_FILESYSTEM_H
