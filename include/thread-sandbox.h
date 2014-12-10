#ifndef CODIUS_THREAD_SANDBOX_H
#define CODIUS_THREAD_SANDBOX_H

#include "sandbox.h"

class ThreadSandbox : public Sandbox {
public:
  ThreadSandbox() : Sandbox() {};
  virtual ~ThreadSandbox() {};

  void spawn(void (*func)(void*), void* data);
};

#endif // CODIUS_THREAD_SANDBOX_H
