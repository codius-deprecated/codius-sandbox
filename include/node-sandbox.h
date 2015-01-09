#ifndef NODE_SANDBOX_H
#define NODE_SANDBOX_H

#include "exec-sandbox.h"
#include <node.h>

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

  void emitEvent(const std::string& name, std::vector<v8::Handle<v8::Value> >& argv);

  void handleExit(int status) override;
  void launchDebugger();
  void handleSignal(int signal) override;
  static void Init(v8::Handle<v8::Object> exports);
  SandboxWrapper* wrap;

private:
    bool m_debuggerOnCrash;
    static v8::Handle<v8::Value> node_spawn(const v8::Arguments& args);
    static v8::Handle<v8::Value> node_kill(const v8::Arguments& args);
    static v8::Handle<v8::Value> node_new(const v8::Arguments& args);
    static v8::Handle<v8::Value> node_getDebugOnCrash(v8::Local<v8::String> property, const v8::AccessorInfo& info);
    static void node_setDebugOnCrash(v8::Local<v8::String> property, v8::Local<v8::Value> value, const v8::AccessorInfo& info);
    static v8::Persistent<v8::Function> s_constructor;
};

#endif // NODE_SANDBOX_H
