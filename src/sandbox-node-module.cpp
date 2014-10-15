#include "sandbox.h"

#include <node/node.h>
#include <v8.h>
#include <memory>
#include <iostream>

using namespace v8;

class NodeSandbox : public Sandbox {
  public:
    SyscallCall handleSyscall(const SyscallCall &call) override {
      return call;
    };
    void handleIPC(const std::vector<char> &request) override {};
    void handleSignal(int signal) override {
      HandleScope scope;
      Handle<Value> argv[2] = {
        String::New("signal"),
        Int32::New(signal)
      };
      node::MakeCallback (m_this, "emit", 2, argv);
      std::cout << "emit signal" << std::endl;
    };

    static void Init(Handle<Object> exports);

  private:
    static Handle<Value> node_spawn(const Arguments& args);
    static Handle<Value> node_kill(const Arguments& args);
    static Handle<Value> node_new(const Arguments& args);
    static Persistent<Function> s_constructor;

    Handle<Object> m_this;
};

struct SandboxWrapper : public node::ObjectWrap {
  std::unique_ptr<NodeSandbox> sbox;
  friend class NodeSandbox;
};

Persistent<Function> NodeSandbox::s_constructor;

Handle<Value>
NodeSandbox::node_kill(const Arguments& args)
{
  SandboxWrapper* wrap;
  wrap = node::ObjectWrap::Unwrap<SandboxWrapper>(args.This());
  wrap->sbox->kill();
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
  std::cout << "Got " << args.Length() << " args" << std::endl;

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
  free (argv);

  return Undefined();
}

Handle<Value> NodeSandbox::node_new(const Arguments& args)
{
  HandleScope scope;

  if (args.IsConstructCall()) {
    SandboxWrapper* wrap = new SandboxWrapper;
    wrap->sbox = std::unique_ptr<NodeSandbox>(new NodeSandbox());
    wrap->Wrap(args.This());
    wrap->sbox->m_this = args.This();
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
