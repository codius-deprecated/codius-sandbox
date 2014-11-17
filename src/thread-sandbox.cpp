#include "thread-sandbox.h"

void
ThreadSandbox::spawn(void (*func)(void*), void* data)
{
  std::vector<char> buf(2048);
  setScratchAddress (reinterpret_cast<Address>(buf.data()));
  if (fork()) {
    traceChild();
  } else {
    setEnteredMain (true);
    func (data);
    exit (0);
  }
}
