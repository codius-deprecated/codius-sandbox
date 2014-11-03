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

  int open(const char* name, int flags) override;
  ssize_t read(int fd, void* buf, size_t count) override;
  int close (int fd) override;
  int fstat (int fd, struct stat* buf) override;
  int getdents (int fd, struct linux_dirent* dirs, unsigned int count) override;

private:
  NodeSandbox* m_sbox;
};

#endif // NODE_FILESYSTEM_H
