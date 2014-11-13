Implemented Syscalls
====================

Codius-sandbox handles a number of syscalls that sandboxed processes have access
to. Any unhandled syscall results in an instantaneous SIGKILL.

The following syscalls interact with the VFS layer, and their behavior is
dependent on any virtual filesystems that are mounted:

- open
- access
- openat
- stat
- read
- close
- ioctl
- fstat
- lseek
- write
- getdents
- getdents64
- readv
- writev
- getcwd
- fcntl
- chdir
- fchdir

Networking emulation layer:

- socket
- connect
- bind
- setsockopt
- getsockname
- getpeername
- getsockopt

Queries about the sandbox system:

- uname
- getrlimit
- getuid
- getgid
- geteuid
- getegid
- getppid
- getpgrp
- getgroups
- getresuid
- getresgid
- capget
- gettid

The following syscalls pass through the sandbox directly to the kernel:

- poll
- mmap
- mprotect
- munmap
- brk
- rt_sigaction
- rt_sigprocmask
- select
- sched_yield
- getpid
- accept
- listen
- exit
- gettimeofday
- tkill
- epoll_create
- restart_syscall
- clock_gettime
- clock_getres
- clock_nanosleep
- exit_group
- epoll_wait
- epoll_ctl
- tgkill
- pselect6
- ppoll
- arch_prctl
- set_robust_list
- get_robust_list
- epoll_pwait
- accept4
- epoll_create1
- pipe2
- futex
