#include <seccomp.h>
#include <error.h>
#include <link.h>
#include <errno.h>

typedef int (*main_t)(int, char**, char**);
main_t realmain;

int wrap_main(int argc, char** argv, char** environ)
{
  scmp_filter_ctx ctx;

  ctx = seccomp_init (SCMP_ACT_KILL);
  seccomp_rule_add (ctx, SCMP_ACT_ALLOW, SCMP_SYS(write), 0);
  seccomp_rule_add (ctx, SCMP_ACT_ALLOW, SCMP_SYS(exit_group), 0);
  seccomp_rule_add (ctx, SCMP_ACT_ALLOW, SCMP_SYS(fstat), 0);
  seccomp_rule_add (ctx, SCMP_ACT_ALLOW, SCMP_SYS(mmap), 0);
  seccomp_load (ctx);
  seccomp_release (ctx);

  return (*realmain)(argc, argv, environ);
}

int __libc_start_main(main_t main,
                      int argc,
                      char ** ubp_av,
                      ElfW(auxv_t) * auxvec,
                      __typeof (main) init,
                      void (*fini) (void),
                      void (*rtld_fini) (void), void * stack_end)
{
  void *libc;
  int (*libc_start_main)(main_t main,
                         int,
                         char **,
                         ElfW(auxv_t) *,
                         __typeof (main),
                         void (*fini) (void),
                         void (*rtld_fini) (void),
                         void * stack_end);
  libc = dlopen("libc.so.6", RTLD_LOCAL | RTLD_LAZY);
  if (!libc)
    error (-1, errno, "dlopen() failed: %s\n", dlerror());
  libc_start_main = dlsym(libc, "__libc_start_main");
  if (!libc_start_main)
    error (-1, errno, "Failed: %s\n", dlerror());
  realmain = main;
  return (*libc_start_main)(wrap_main, argc, ubp_av, auxvec, init, fini, rtld_fini, stack_end);
}
