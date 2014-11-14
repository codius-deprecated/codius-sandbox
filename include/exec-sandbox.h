#ifndef CODIUS_EXEC_SANDBOX_H
#define CODIUS_EXEC_SANDBOX_H

#include "sandbox.h"

class ExecSandbox : public Sandbox {
public:
  ExecSandbox() : Sandbox() {};
  virtual ~ExecSandbox() {};
  /**
   * Spawns a binary inside this sandbox. Arguments are the same as for
   * execv(3)
   */
  void spawn(char** argv, std::map<std::string, std::string>& envp);
  void handleExecEvent(pid_t pid) override;

private:
  void execChild(char** argv, std::map<std::string, std::string>& envp) __attribute__ ((noreturn));
  void findScratchBuffer(pid_t pid);
};

#endif // CODIUS_EXEC_SANDBOX_H
