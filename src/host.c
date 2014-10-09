#include <unistd.h>
#include <stdlib.h>
#include <memory.h>
#include <errno.h>
#include <error.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ptrace.h>
#include <sys/user.h>

#define ORIG_EAX 11

#define PTRACE_EVENT_SECCOMP 7

int main(int argc, char** argv)
{
  pid_t child;

  child = fork();
  if (child) {
    int status;

    ptrace (PTRACE_ATTACH, child, 0, 0);
    waitpid (child, &status, 0);
    ptrace (PTRACE_SETOPTIONS, child, 0,
        PTRACE_O_EXITKILL | PTRACE_O_TRACESYSGOOD );
    ptrace (PTRACE_SYSCALL, child, 0, 0);

    while (!WIFEXITED (status)) {
      waitpid (child, &status, 0);
      if (WSTOPSIG (status) == (SIGTRAP | 0x80)) {
        struct user_regs_struct regs;
        memset (&regs, 0, sizeof (regs));
        if (ptrace (PTRACE_GETREGS, child, 0, &regs) < 0) {
          error (-1, errno, "Failed to fetch registers");
        }
        printf ("Sandboxed module tried syscall %ld\n", regs.orig_rax);
        ptrace (PTRACE_SETREGS, child, &regs, 0);
        ptrace (PTRACE_SYSCALL, child, 0, 0);
        waitpid (child, &status, 0);
        ptrace (PTRACE_SYSCALL, child, 0, 0);
      }
      ptrace (PTRACE_CONT, child, 0, 0);
    }
    printf ("Sandboxed module exited: %d\n", WEXITSTATUS (status));
    return WEXITSTATUS (status);
  } else {
    printf ("Launching %s\n", argv[1]);
    ptrace (PTRACE_TRACEME, 0, 0);
    raise (SIGSTOP);
    if (execvp (argv[1], &argv[1]) < 0) {
      error(-1, errno, "Could not start sandboxed module:");
    }
  }
}
