#include "node-filesystem.h"
#include "node-sandbox.h"
#include <future>
#include <iostream>
#include <memory.h>

using namespace v8;

CodiusNodeFilesystem::CodiusNodeFilesystem(NodeSandbox* sbox)
  : Filesystem(),
    m_sbox (sbox) {}

CodiusNodeFilesystem::VFSResult
CodiusNodeFilesystem::doVFS(const std::string& name, Handle<Value> argv[], int argc)
{
  uv_loop_t*  loop = uv_default_loop ();
  Handle<Value> result;
  NodeSandbox::VFSFuture ret = m_sbox->doVFS (name, argv, argc);

  while (ret.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
    uv_run (loop, UV_RUN_ONCE);

  result = ret.get();

  if (result->IsObject()) {
    Handle<Object> resultObj = result->ToObject();
    int err = resultObj->Get (String::NewSymbol ("error"))->ToInt32()->Value();
    Handle<Value> resultValue = resultObj->Get (String::NewSymbol ("result"));

    VFSResult r  = {
      .errnum = err,
      .result = resultValue
    };

    return r;
  }

  ThrowException(Exception::TypeError(String::New("Expected a VFS call return type")));

  VFSResult r = {
    .errnum = ENOSYS,
    .result = Undefined()
  };

  return r;
}

int
CodiusNodeFilesystem::open(const char* name, int flags, int mode)
{
  Handle<Value> argv[] = {
    String::New (name),
    Int32::New (flags),
    Int32::New (mode)
  };

  VFSResult ret = doVFS(std::string ("open"), argv, 3);

  if (ret.errnum) {
    return -ret.errnum;
  }

  return ret.result->ToInt32()->Value();
}

ssize_t
CodiusNodeFilesystem::read(int fd, void* buf, size_t count)
{
  Handle<Value> argv[] = {
    Int32::New (fd),
    Int32::New (count)
  };

  VFSResult ret = doVFS (std::string ("read"), argv, 2);

  if (ret.errnum)
    return -ret.errnum;

  ret.result->ToString()->WriteUtf8 (static_cast<char*>(buf), count);

  return ret.result->ToString()->Utf8Length();
}

int
CodiusNodeFilesystem::close(int fd)
{
  Handle<Value> argv[] = {
    Int32::New (fd)
  };

  return -doVFS (std::string ("close"), argv, 1).errnum;
}

int
CodiusNodeFilesystem::fstat(int fd, struct stat* buf)
{
  Handle<Value> argv[] = {
    Int32::New (fd)
  };
  VFSResult ret = doVFS (std::string ("fstat"), argv, 1);

  if (ret.errnum) 
    return -ret.errnum;

  Handle<Object> statObj = ret.result->ToObject();
  buf->st_dev = statObj->Get(String::NewSymbol("dev"))->ToInt32()->Value();
  buf->st_ino = statObj->Get(String::NewSymbol("ino"))->ToInt32()->Value();
  buf->st_mode = statObj->Get(String::NewSymbol("mode"))->ToInt32()->Value();
  buf->st_nlink = statObj->Get(String::NewSymbol("nlink"))->ToInt32()->Value();
  buf->st_uid = statObj->Get(String::NewSymbol("uid"))->ToInt32()->Value();
  buf->st_gid = statObj->Get(String::NewSymbol("gid"))->ToInt32()->Value();
  buf->st_rdev = statObj->Get(String::NewSymbol("rdev"))->ToInt32()->Value();
  buf->st_size = statObj->Get(String::NewSymbol("size"))->ToInt32()->Value();
  buf->st_blksize = statObj->Get(String::NewSymbol("blksize"))->ToInt32()->Value();
  buf->st_blocks = statObj->Get(String::NewSymbol("blocks"))->ToInt32()->Value();

  return 0;
}

off_t
CodiusNodeFilesystem::lseek(int fd, off_t offset, int whence)
{
  // FIXME: node's FS module doesn't implement lseek, but we need it for
  // rewinddir()
  return -ENOSYS;
}

int
CodiusNodeFilesystem::getdents(int fd, struct linux_dirent* dirs, unsigned int count)
{
  Handle<Value> argv[] = {
    Int32::New (fd)
  };
  VFSResult ret = doVFS(std::string ("getdents"), argv, 1);
  if (ret.errnum)
    return -ret.errnum;

  DirentBuilder builder;

  Handle<Array> fileList = Handle<Array>::Cast (ret.result);
  for (uint32_t i = 0; i < fileList->Length(); i++) {
    Handle<String> filename = fileList->Get(i)->ToString();
    std::vector<char> buf (filename->Utf8Length()+1);
    filename->WriteUtf8 (buf.data(), buf.size());
    builder.append (std::string (buf.data()));
  }
  std::vector<char> buf;
  buf = builder.data();
  memcpy (dirs, buf.data(), buf.size());
  return buf.size();
}

ssize_t
CodiusNodeFilesystem::write(int fd, void* buf, size_t count)
{
  Handle<Value> argv[] = {
    Int32::New (fd),
    String::New ((char*)buf, count)
  };

  VFSResult ret = doVFS (std::string ("write"), argv, 2);

  if (ret.errnum)
    return -ret.errnum;

  return ret.result->ToInt32()->Value();
}

ssize_t
CodiusNodeFilesystem::readlink (const char* path, char* buf, size_t bufsize)
{
  Handle<Value> argv[] = {
    String::New (path)
  };

  VFSResult ret = doVFS (std::string ("readlink"), argv, 1);

  if (ret.errnum)
    return -ret.errnum;

  ret.result->ToString()->WriteUtf8 (buf, bufsize);
  return ret.result->ToString()->Utf8Length();
}

int
CodiusNodeFilesystem::access (const char* name, int mode)
{
  Handle<Value> argv[] = {
    String::New (name),
    Int32::New (mode)
  };

  VFSResult ret = doVFS (std::string ("access"), argv, 2);

  if (ret.errnum)
    return -ret.errnum;

  return ret.result->ToInt32()->Value();
}

int
CodiusNodeFilesystem::stat (const char* name, struct stat* buf)
{
  Handle<Value> argv[] = {
    String::New (name)
  };

  VFSResult ret = doVFS (std::string ("stat"), argv, 1);

  if (ret.errnum) 
    return -ret.errnum;

  Handle<Object> statObj = ret.result->ToObject();
  buf->st_dev = statObj->Get(String::NewSymbol("dev"))->ToInt32()->Value();
  buf->st_ino = statObj->Get(String::NewSymbol("ino"))->ToInt32()->Value();
  buf->st_mode = statObj->Get(String::NewSymbol("mode"))->ToInt32()->Value();
  buf->st_nlink = statObj->Get(String::NewSymbol("nlink"))->ToInt32()->Value();
  buf->st_uid = statObj->Get(String::NewSymbol("uid"))->ToInt32()->Value();
  buf->st_gid = statObj->Get(String::NewSymbol("gid"))->ToInt32()->Value();
  buf->st_rdev = statObj->Get(String::NewSymbol("rdev"))->ToInt32()->Value();
  buf->st_size = statObj->Get(String::NewSymbol("size"))->ToInt32()->Value();
  buf->st_blksize = statObj->Get(String::NewSymbol("blksize"))->ToInt32()->Value();
  buf->st_blocks = statObj->Get(String::NewSymbol("blocks"))->ToInt32()->Value();

  return 0;
}

int
CodiusNodeFilesystem::lstat (const char* name, struct stat* buf)
{
  Handle<Value> argv[] = {
    String::New (name)
  };

  VFSResult ret = doVFS (std::string ("lstat"), argv, 1);

  if (ret.errnum) 
    return -ret.errnum;

  Handle<Object> statObj = ret.result->ToObject();
  buf->st_dev = statObj->Get(String::NewSymbol("dev"))->ToInt32()->Value();
  buf->st_ino = statObj->Get(String::NewSymbol("ino"))->ToInt32()->Value();
  buf->st_mode = statObj->Get(String::NewSymbol("mode"))->ToInt32()->Value();
  buf->st_nlink = statObj->Get(String::NewSymbol("nlink"))->ToInt32()->Value();
  buf->st_uid = statObj->Get(String::NewSymbol("uid"))->ToInt32()->Value();
  buf->st_gid = statObj->Get(String::NewSymbol("gid"))->ToInt32()->Value();
  buf->st_rdev = statObj->Get(String::NewSymbol("rdev"))->ToInt32()->Value();
  buf->st_size = statObj->Get(String::NewSymbol("size"))->ToInt32()->Value();
  buf->st_blksize = statObj->Get(String::NewSymbol("blksize"))->ToInt32()->Value();
  buf->st_blocks = statObj->Get(String::NewSymbol("blocks"))->ToInt32()->Value();

  return 0;
}
