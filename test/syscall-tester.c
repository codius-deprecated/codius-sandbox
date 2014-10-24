#include <unistd.h>
#include <sys/syscall.h>
#include <stdlib.h>
#include <errno.h>
#include <memory.h>

int main(int argc, char** argv)
{
  int callNum = atoi (argv[1]);
  int args[5];

  memset (args, 0, sizeof (args));
  syscall (callNum, args[0], args[1], args[2], args[3], args[4], args[5]);

  return errno;
}
