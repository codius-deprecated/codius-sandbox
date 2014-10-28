Codius Sandbox
==============

Codius Sandbox is a small C++ library and node module that uses seccomp to
execute untrusted code in a secure sandbox, in the same vein of User Mode Linux,
Native Client, and others.

Dependencies
------------

* cppunit
* libuv
* libseccomp
* A compiler that supports C++11, such as GCC 4.8 or Clang.

On Ubuntu, these can be installed with:

```apt-get install libuv-dev libseccomp-dev libcppunit-dev```
