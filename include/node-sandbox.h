#ifndef NODE_SANDBOX_H
#define NODE_SANDBOX_H

#include "exec-sandbox.h"
#include <node.h>
#include <memory>
#include <vector>
#include <future>

class NodeSandbox;

class SandboxWrapper : public node::ObjectWrap {
  public:
    SandboxWrapper();
    ~SandboxWrapper();
    std::unique_ptr<NodeSandbox> sbox;
    v8::Persistent<v8::Object> nodeThis;
    friend class NodeSandbox;
};

class NodeSandbox : public ExecSandbox {
public:
  NodeSandbox(SandboxWrapper* _wrap);

  std::vector<char> mapFilename(std::vector<char> fname);
  void emitEvent(const std::string& name, std::vector<v8::Handle<v8::Value> >& argv);
  SyscallCall mapFilename(const SyscallCall& call);
  SyscallCall handleSyscall(const SyscallCall &call) override;

  using VFSPromise = std::promise<v8::Persistent<v8::Value> >;
  using VFSFuture = std::shared_future<v8::Persistent<v8::Value> >;
  VFSFuture doVFS(const std::string& name, v8::Handle<v8::Value> argv[], int argc);

  void handleIPC(codius_request_t* request) override;
  void handleExit(int status) override;
  void launchDebugger();
  void handleSignal(int signal) override;
  static void Init(v8::Handle<v8::Object> exports);
  SandboxWrapper* wrap;

private:
    bool m_debuggerOnCrash;
    static v8::Handle<v8::Value> node_spawn(const v8::Arguments& args);
    static v8::Handle<v8::Value> node_kill(const v8::Arguments& args);
    static v8::Handle<v8::Value> node_finish_ipc(const v8::Arguments& args);
    static v8::Handle<v8::Value> node_finish_vfs(const v8::Arguments& args);
    static v8::Handle<v8::Value> node_new(const v8::Arguments& args);
    static v8::Handle<v8::Value> node_getDebugOnCrash(v8::Local<v8::String> property, const v8::AccessorInfo& info);
    static v8::Handle<v8::Value> node_addToWhitelist(const v8::Arguments& args);
    static void node_setDebugOnCrash(v8::Local<v8::String> property, v8::Local<v8::Value> value, const v8::AccessorInfo& info);
    static v8::Persistent<v8::Function> s_constructor;
};

#endif // NODE_SANDBOX_H
