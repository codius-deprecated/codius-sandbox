#ifndef NODE_MODULE_H
#define NODE_MODULE_H

#include <node.h>

extern "C" {
  struct Sandbox;
  extern Sandbox* sandbox_new();
  extern void sandbox_free(Sandbox*);
  extern void sandbox_spawn(Sandbox*, char** argv);
  extern void sandbox_tick(Sandbox*);
}

class NodeSandbox;
class SandboxDeleter {
  public:
    void operator()(Sandbox* s);
};

class NodeSandbox {
public:
  NodeSandbox();
  ~NodeSandbox();
  static void Init(v8::Handle<v8::Object> exports);

private:
  static v8::Handle<v8::Value> node_spawn(const v8::Arguments& args);
  static v8::Handle<v8::Value> node_kill(const v8::Arguments& args);
  static v8::Handle<v8::Value> node_finish_ipc(const v8::Arguments& args);
  static v8::Handle<v8::Value> node_new(const v8::Arguments& args);
  static v8::Persistent<v8::Function> s_constructor;
  static void cb_signal (uv_signal_t* handle, int signum);
  std::unique_ptr<Sandbox, SandboxDeleter> m_box;
  uv_signal_t m_signal;
};

class SandboxWrapper : public node::ObjectWrap {
  public:
    SandboxWrapper();
    ~SandboxWrapper();
    std::unique_ptr<NodeSandbox> sbox;
    friend class NodeSandbox;
};

#endif // NODE_MODULE_H
