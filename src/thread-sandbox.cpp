#include "thread-sandbox.h"

void
ThreadSandbox::spawn(void (*func)(void*), void* data)
{
  if (fork()) {
    traceChild();
  } else {
    setEnteredMain (true);
    func (data);
    exit (0);
  }
}
