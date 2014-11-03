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
CodiusNodeFilesystem::open(const char* name, int flags)
{
  Handle<Value> argv[] = {
    String::New (name),
    Int32::New (flags),
    Int32::New (0) //FIXME: We also need the mode?
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
    char buf[filename->Utf8Length()];
    filename->WriteUtf8 (buf, filename->Utf8Length());
    builder.append (std::string (buf));
  }
  std::vector<char> buf;
  buf = builder.data();
  memcpy (dirs, buf.data(), buf.size());
  return buf.size();
}

