#include "sandbox.h"

#include <node/node.h>
#include <v8.h>
#include <memory>
#include <iostream>
#include <asm/unistd.h>

using namespace v8;

class NodeSandbox;

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
      : wrap(_wrap)
    {}

    SyscallCall handleSyscall(const SyscallCall &call) override {
      SyscallCall ret (call);
      HandleScope scope;
      Handle<Array> argStruct = Array::New();
      Handle<Object> callStruct = Object::New();
      for (int i = 0; i < 6; i++) {
        argStruct->Set(i, Int32::New(call.args[i]));
      }
      callStruct->Set (String::NewSymbol ("id"), Int32::New (call.id));
      callStruct->Set (String::NewSymbol ("arguments"), argStruct);
      Handle<Value> argv[1] = {
        callStruct
      };
      Handle<Value> callbackRet = node::MakeCallback (wrap->nodeThis, "handleSyscall", 1, argv);
      if (callbackRet->IsObject()) {
        Handle<Object> callbackObj = callbackRet->ToObject();
        Handle<Object> callbackArgs = callbackObj->Get(String::NewSymbol ("arguments"))->ToObject();
        if (callbackArgs->IsArray()) {
          ret.id = callbackObj->Get(String::NewSymbol ("id"))->ToInt32()->Value();
          for (int i = 0; i < 6; i++) {
            if (callbackArgs->Get(i)->IsInt32())
              ret.args[i] = callbackArgs->Get(i)->ToInt32()->Value();
            else
              ThrowException(Exception::TypeError(String::New("Expected a syscall call return type")));
          }
        } else {
          ThrowException(Exception::TypeError(String::New("Expected a syscall call return type")));
        }
      } else {
        ThrowException(Exception::TypeError(String::New("Expected a syscall call return type")));
        ret.id = -1;
      }

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
          std::cout << "Redirected to " << buf.data() << std::endl;
          writeScratch (buf.size(), buf.data());
          ret.args[0] = getScratchAddress();
        } else {
          ThrowException(Exception::TypeError(String::New("Expected a string return value")));
          ret.id = -1;
        }
      }
      return ret;
    };

    void handleIPC(const std::vector<char> &request) override {
      Handle<Value> argv[1] = {
        String::New(request.data())
      };
      Handle<Value> callbackRet = node::MakeCallback (wrap->nodeThis, "handleIPC", 1, argv);
      if (callbackRet->IsObject()) {
        Handle<Object> callbackObj = callbackRet->ToObject();
      } else {
        ThrowException(Exception::TypeError(String::New("Expected an IPC call return type")));
      }
    };

    void handleExit(int status) override {
      HandleScope scope;
      Handle<Value> argv[2] = {
        String::NewSymbol("exit"),
        Int32::New(status)
      };
      node::MakeCallback (wrap->nodeThis, "emit", 2, argv);
    }

    void handleSignal(int signal) override {
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
      argv[i] = static_cast<char*>(calloc(sizeof(char), v->Utf8Length()));
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
