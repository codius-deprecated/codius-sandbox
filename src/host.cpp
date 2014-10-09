#include "sandbox.h"

#include <memory>

int main(int argc, char** argv)
{
  std::unique_ptr<Sandbox> sandbox = std::unique_ptr<Sandbox>(new Sandbox());
  return sandbox->exec (&argv[1]);
}
