#include "sandbox.h"
#include "sandbox-ipc.h"

#include <node/node.h>
#include <v8.h>
#include <memory>
#include <iostream>
#include <asm/unistd.h>
#include <error.h>
#include <sys/un.h>

using namespace v8;

class NodeSandbox;

//static void handle_stdio_read (SandboxIPC& ipc, void* user_data);

struct NodeIPC : public SandboxIPC {
  using Ptr = std::unique_ptr<NodeIPC>;

  NodeIPC(int _dupAs, Handle<Object> streamObj);
  void onReadReady() override;
private:
  Persistent<Object> nodeThis;
};

NodeIPC::NodeIPC (int _dupAs, Handle<Object> streamObj)
  : SandboxIPC (_dupAs),
    nodeThis (streamObj)
{
}

void
NodeIPC::onReadReady()
{
  char buf[1024];
  ssize_t readSize;

  readSize = read (parent, buf, sizeof (buf)-1);
  buf[readSize] = 0;

  Handle<Value> argv[2] = {
    Int32::New (dupAs),
    String::New (buf)
  };

  node::MakeCallback (nodeThis, "onData", 2, argv);
}

class SandboxWrapper : public node::ObjectWrap {
  public:
    SandboxWrapper();
    ~SandboxWrapper();
    std::unique_ptr<NodeSandbox> sbox;
    Persistent<Object> nodeThis;
    friend class NodeSandbox;
};

class NodeSandbox : public Sandbox {
  public:
    NodeSandbox(SandboxWrapper* _wrap)
      : wrap(_wrap),
        m_debuggerOnCrash(false)
    {}

    std::vector<char> mapFilename(std::vector<char> fname)
    {
      Handle<Value> argv[1] = {
        String::NewSymbol (fname.data())
      };
      Handle<Value> callbackRet = node::MakeCallback (wrap->nodeThis, "mapFilename", 1, argv);
      if (callbackRet->IsString()) {
        std::vector<char> buf;
        buf.resize (callbackRet->ToString()->Utf8Length()+1);
        callbackRet->ToString()->WriteUtf8 (buf.data());
        buf[buf.size()-1] = 0;
        return buf;
      } else {
        ThrowException(Exception::TypeError(String::New("Expected a string return value")));
      }
      return std::vector<char>();
    }

    void emitEvent(const std::string& name, std::vector<Handle<Value> >& argv) {
      std::vector<Handle<Value> >  args;
      args.push_back (String::NewSymbol (name.c_str()));
      args.insert (args.end(), argv.begin(), argv.end());
      node::MakeCallback (wrap->nodeThis, "emit", args.size(), args.data());
    }

    SyscallCall mapFilename(const SyscallCall& call) {
      SyscallCall ret (call);
      std::vector<char> fname (1024);
      copyString (call.args[0], fname.size(), fname.data());
      fname = mapFilename (fname);
      if (fname.size()) {
        ret.args[0] = writeScratch (fname.size(), fname.data());
      } else {
        ret.id = -1;
      }
      return ret;
    }

    SyscallCall handleSyscall(const SyscallCall &call) override {
      SyscallCall ret (call);

      if (ret.id == __NR_open || ret.id == __NR_stat || ret.id == __NR_access || ret.id == __NR_readlink || ret.id == __NR_lstat) {
        ret = mapFilename (ret);
      } else if (ret.id == __NR_getsockname) {
        //FIXME: Should return what was originally passed in via bind() or
        //similar
      } else if (ret.id == __NR_getsockopt) {
        //FIXME: Needs emulation
      } else if (ret.id == __NR_setsockopt) {
        //FIXME: Needs emulation
      } else if (ret.id == __NR_bind) {
        struct sockaddr_un addr;
        addr.sun_family = AF_UNIX;
        snprintf (addr.sun_path, sizeof (addr.sun_path), "/tmp/codius-sandbox-socket-%d-%d", getChildPID(), static_cast<int>(ret.args[0]));
        ret.args[1] = writeScratch (sizeof (addr), reinterpret_cast<char*>(&addr));
        ret.args[2] = sizeof (addr);
        std::vector<Handle<Value> > args = {
          String::New (addr.sun_path)
        };
        emitEvent ("newSocket", args);
      } else if (ret.id == __NR_socket) {
        ret.args[0] = AF_UNIX;
      } else if (ret.id == __NR_execve) {
        kill();
      } else {
        std::cout << "try " << call.id << std::endl;
      }
      return ret;
    };

    codius_result_t* handleIPC(codius_request_t* request) override {
      Handle<Value> argv[2] = {
        String::New(request->api_name),
        String::New(request->method_name)
      };
      Handle<Value> callbackRet = node::MakeCallback (wrap->nodeThis, "handleIPC", 2, argv);
      if (callbackRet->IsObject()) {
        //TODO: implement IPC :)
        //Handle<Object> callbackObj = callbackRet->ToObject();
      } else {
        ThrowException(Exception::TypeError(String::New("Expected an IPC call return type")));
      }
      return NULL;
    };

    void handleExit(int status) override {
      std::vector<Handle<Value> > args = {
        Int32::New (status)
      };
      emitEvent ("exit", args);
    }

    void launchDebugger() {
      releaseChild (SIGSTOP);
      char pidStr[15];
      snprintf (pidStr, sizeof (pidStr), "%d", getChildPID());
      // libuv apparently sets O_CLOEXEC, just to frustrate us if we want to
      // break out
      fcntl (0, F_SETFD, 0);
      fcntl (1, F_SETFD, 0);
      fcntl (2, F_SETFD, 0);
      if (execlp ("gdb", "gdb", "-p", pidStr, NULL) < 0) {
        error (EXIT_FAILURE, errno, "Could not start debugger");
      }
    }

    void handleSignal(int signal) override {
      if (m_debuggerOnCrash && signal == SIGSEGV) {
        launchDebugger();
      }
      std::vector<Handle<Value> > args = {
        Int32::New(signal)
      };
      emitEvent ("signal", args);
    };

    static void Init(Handle<Object> exports);
    SandboxWrapper* wrap;

  private:
      bool m_debuggerOnCrash;
      static Handle<Value> node_spawn(const Arguments& args);
      static Handle<Value> node_kill(const Arguments& args);
      static Handle<Value> node_new(const Arguments& args);
      static Handle<Value> node_getDebugOnCrash(Local<String> property, const AccessorInfo& info);
      static void node_setDebugOnCrash(Local<String> property, Local<Value> value, const AccessorInfo& info);
      static Persistent<Function> s_constructor;
};

Persistent<Function> NodeSandbox::s_constructor;

Handle<Value>
NodeSandbox::node_kill(const Arguments& args)
{
  SandboxWrapper* wrap;
  wrap = node::ObjectWrap::Unwrap<SandboxWrapper>(args.This());
  wrap->sbox->kill();
  return Undefined();
}

/*static void
handle_stdio_read (SandboxIPC& ipc, void* data)
{
  std::vector<char> buf(2048);
  int bytesRead;

  if ((bytesRead = read (ipc.parent, buf.data(), buf.size()))<0) {
    error (EXIT_FAILURE, errno, "Couldn't read stderr");
  }

  buf.resize (bytesRead);

  std::cout << "stderr: " << buf.data() << std::endl;
}*/

Handle<Value>
NodeSandbox::node_spawn(const Arguments& args)
{
  HandleScope scope;
  char** argv;
  SandboxWrapper* wrap;

  wrap = node::ObjectWrap::Unwrap<SandboxWrapper>(args.This());
  argv = static_cast<char**>(calloc (sizeof (char*), args.Length()+1));
  argv[args.Length()] = nullptr;

  for(int i = 0; i < args.Length(); i++) {
    if (args[i]->IsString()) {
      Local<String> v = args[i]->ToString();
      argv[i] = static_cast<char*>(calloc(sizeof(char), v->Utf8Length()+1));
      v->WriteUtf8(argv[i]);
    } else {
      ThrowException(Exception::TypeError(String::New("Arguments must be strings.")));
      goto out;
    }
  }

  wrap->sbox->spawn(argv);

out:
  for (int i = 0; i < args.Length();i ++) {
    free (argv[i]);
  }
  free (argv);

  return Undefined();
}

SandboxWrapper::SandboxWrapper()
  : sbox (new NodeSandbox(this))
{}

SandboxWrapper::~SandboxWrapper()
{}

Handle<Value>
NodeSandbox::node_getDebugOnCrash(Local<String> property, const AccessorInfo& info)
{
  SandboxWrapper* wrap;
  wrap = node::ObjectWrap::Unwrap<SandboxWrapper>(info.This());
  return Boolean::New (wrap->sbox->m_debuggerOnCrash);
}

void
NodeSandbox::node_setDebugOnCrash(Local<String> property, Local<Value> value, const AccessorInfo& info)
{
  SandboxWrapper* wrap;
  wrap = node::ObjectWrap::Unwrap<SandboxWrapper>(info.This());
  wrap->sbox->m_debuggerOnCrash = value->ToBoolean()->Value();
}

Handle<Value> NodeSandbox::node_new(const Arguments& args)
{
  HandleScope scope;

  if (args.IsConstructCall()) {
    SandboxWrapper* wrap = new SandboxWrapper();
    wrap->Wrap(args.This());
    wrap->nodeThis = wrap->handle_;
    node::MakeCallback (wrap->nodeThis, "_init", 0, nullptr);
    wrap->nodeThis->SetAccessor (String::NewSymbol ("debuggerOnCrash"), NodeSandbox::node_getDebugOnCrash, NodeSandbox::node_setDebugOnCrash);
    wrap->sbox->addIPC (std::unique_ptr<NodeIPC> (new NodeIPC (STDOUT_FILENO, wrap->handle_)));
    wrap->sbox->addIPC (std::unique_ptr<NodeIPC> (new NodeIPC (STDERR_FILENO, wrap->handle_)));

    return args.This();
  } else {
    Local<Value> argv[1] = { args[0] };
    return scope.Close(s_constructor->NewInstance(1, argv));
  }
}

void
NodeSandbox::Init(Handle<Object> exports)
{
  Local<FunctionTemplate> tpl = FunctionTemplate::New(node_new);
  tpl->SetClassName(String::NewSymbol("Sandbox"));
  tpl->InstanceTemplate()->SetInternalFieldCount(2);
  node::SetPrototypeMethod(tpl, "spawn", node_spawn);
  node::SetPrototypeMethod(tpl, "kill", node_kill);
  s_constructor = Persistent<Function>::New(tpl->GetFunction());
  exports->Set(String::NewSymbol("Sandbox"), s_constructor);

  Local<FunctionTemplate> channelTpl = FunctionTemplate::New(node_new);
  channelTpl->SetClassName (String::NewSymbol ("Channel"));
  channelTpl->InstanceTemplate()->SetInternalFieldCount (2);
}

void
init(Handle<Object> exports) {
  NodeSandbox::Init(exports);
}

NODE_MODULE (codius_sandbox, init);
