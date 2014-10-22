#include "sandbox.h"
#include "sandbox-ipc.h"

#include <node/node.h>
#include <v8.h>
#include <memory>
#include <iostream>
#include <asm/unistd.h>
#include <error.h>

using namespace v8;

class NodeSandbox;

static void handle_stdio_read (SandboxIPC& ipc, void* user_data);

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

    SyscallCall handleSyscall(const SyscallCall &call) override {
      SyscallCall ret (call);

      if (ret.id == __NR_open) {
        char filename[1024];
        copyString (ret.args[0], 1024, filename);
        Handle<Value> argv[1] = {
          String::NewSymbol (filename)
        };
        Handle<Value> callbackRet = node::MakeCallback (wrap->nodeThis, "mapFilename", 1, argv);
        if (callbackRet->IsString()) {
          std::vector<char> buf;
          buf.resize (callbackRet->ToString()->Utf8Length()+1);
          callbackRet->ToString()->WriteUtf8 (buf.data());
          buf[buf.size()-1] = 0;
          writeScratch (buf.size(), buf.data());
          ret.args[0] = getScratchAddress();
        } else {
          ThrowException(Exception::TypeError(String::New("Expected a string return value")));
          ret.id = -1;
        }
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
      HandleScope scope;
      Handle<Value> argv[2] = {
        String::NewSymbol("exit"),
        Int32::New(status)
      };
      node::MakeCallback (wrap->nodeThis, "emit", 2, argv);
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
      HandleScope scope;
      Handle<Value> argv[2] = {
        String::NewSymbol("signal"),
        Int32::New(signal)
      };
      node::MakeCallback (wrap->nodeThis, "emit", 2, argv);
    };

    static void Init(Handle<Object> exports);
    SandboxWrapper* wrap;

  private:
      bool m_debuggerOnCrash;
      static Handle<Value> node_spawn(const Arguments& args);
      static Handle<Value> node_kill(const Arguments& args);
      static Handle<Value> node_new(const Arguments& args);
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

static void
handle_stdio_read (SandboxIPC& ipc, void* data)
{
  std::vector<char> buf(2048);
  int bytesRead;

  if ((bytesRead = read (ipc.parent, buf.data(), buf.size()))<0) {
    error (EXIT_FAILURE, errno, "Couldn't read stderr");
  }

  buf.resize (bytesRead);

  std::cout << "stderr: " << buf.data() << std::endl;
}


Handle<Value>
NodeSandbox::node_spawn(const Arguments& args)
{
  HandleScope scope;
  char** argv;
  SandboxWrapper* wrap;
  SandboxIPC::Ptr stdoutSocket(new SandboxIPC (STDOUT_FILENO));
  SandboxIPC::Ptr stderrSocket(new SandboxIPC (STDERR_FILENO));

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

  stdoutSocket->setCallback(handle_stdio_read, wrap);
  stderrSocket->setCallback(handle_stdio_read, wrap);
  wrap->sbox->addIPC (std::move (stdoutSocket));
  wrap->sbox->addIPC (std::move (stderrSocket));

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

Handle<Value> NodeSandbox::node_new(const Arguments& args)
{
  HandleScope scope;

  if (args.IsConstructCall()) {
    SandboxWrapper* wrap = new SandboxWrapper();
    wrap->Wrap(args.This());
    wrap->nodeThis = wrap->handle_;
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
}

void
init(Handle<Object> exports) {
  NodeSandbox::Init(exports);
}

NODE_MODULE (codius_sandbox, init);
