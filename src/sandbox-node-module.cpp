#include "node-sandbox.h"

#include "vfs.h"
#include "process-reader.h"

#include <asm/unistd.h>
#include <error.h>
#include <sys/un.h>
#include <unistd.h>

using namespace v8;

NodeSandbox::NodeSandbox(SandboxWrapper* _wrap)
  : wrap(_wrap),
    m_debuggerOnCrash(false)
{
}

void
NodeSandbox::emitEvent(const std::string& name, std::vector<Handle<Value> >& argv)
{
  std::vector<Handle<Value> >  args;
  args.push_back (String::NewSymbol (name.c_str()));
  args.insert (args.end(), argv.begin(), argv.end());
  node::MakeCallback (wrap->nodeThis, "emit", args.size(), args.data());
}

void
NodeSandbox::handleExit(int status)
{
  std::vector<Handle<Value> > args = {
    Int32::New (status)
  };
  emitEvent ("exit", args);
}

void
NodeSandbox::launchDebugger() {
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

void
NodeSandbox::handleSignal(int signal)
{
  if (m_debuggerOnCrash && (signal == SIGSEGV || signal == SIGABRT || signal == SIGTRAP)) {
    launchDebugger();
  }
  std::vector<Handle<Value> > args = {
    Int32::New(signal)
  };
  emitEvent ("signal", args);
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
  std::map<std::string, std::string> envp;
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
      if (i <= args.Length() - 2 ) {
        ThrowException(Exception::TypeError(String::New("Arguments must be strings.")));
        goto out;
      } else {
        // Last parameter is an options structure
        Local<Object> options = args[i]->ToObject();
        if (!options.IsEmpty()) {
          if (options->HasRealNamedProperty(String::NewSymbol("env"))) {
            Local<Object> envOptions = options->Get(String::NewSymbol("env"))->ToObject();
            if (!envOptions.IsEmpty()) {
              Local<Array> envArray = envOptions->GetOwnPropertyNames();
              for (uint32_t i = 0; i < envArray->Length(); i++) {
                std::vector<char> strName;
                std::vector<char> strValue;
                Local<String> envName;
                Local<String> envValue;

                if (!(envArray->Get(i)->IsString() && envArray->Get(i)->IsString()))
                  goto err_env;

                envName = envArray->Get(i)->ToString();
                envValue = envOptions->Get(envName)->ToString();

                strName.resize (envName->Utf8Length()+1);
                strValue.resize (envValue->Utf8Length()+1);
                envName->WriteUtf8 (strName.data());
                envValue->WriteUtf8 (strValue.data());
                envp.insert (std::make_pair(std::string (strName.data()), std::string(strValue.data())));
              }
            } else {
              goto err_env;
            }
          }
        } else {
          goto err_options;
        }
      }
    }
  }

  wrap->sbox->getVFS().setCWD ("/contract/");
  wrap->sbox->spawn(argv, envp);

  goto out;

err_env:
  ThrowException(Exception::TypeError(String::New("'env' option must be a map of string:string")));
  goto out;

err_options:
  ThrowException(Exception::TypeError(String::New("Last argument must be an options structure.")));
  goto out;

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

NODE_MODULE (node_codius_sandbox, init);
