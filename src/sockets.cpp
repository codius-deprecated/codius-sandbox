#include "sockets.h"
#include "native-sockets.h"

#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cassert>

#include "debug.h"

Sockets::Sockets(Sandbox* sandbox) :
  m_sbox (sandbox)
{
  registerBackend (SocketBackend::Ptr(new NativeSockets()), -1, -1, -1);
}

void
Sockets::registerBackend(SocketBackend::Ptr backend, int domain, int type, int protocol)
{
  BackendRegistration reg = std::make_pair (SocketType (domain, type, protocol), backend);
  m_backends.push_back (reg);
}

#define HANDLE_CALL(x) case SYS_##x: Debug() << "Sockets::" #x;do_##x(ret);break;

Sandbox::SyscallCall
Sockets::handleSyscall(const Sandbox::SyscallCall& call)
{
  Sandbox::SyscallCall ret(call);
  switch (call.id) {
    HANDLE_CALL (socket);
    HANDLE_CALL (bind);
    HANDLE_CALL (listen);
    HANDLE_CALL (accept);
    HANDLE_CALL (close);
    HANDLE_CALL (setsockopt);
  }
  return ret;
}

#undef HANDLE_CALL

void
Sockets::do_close(Sandbox::SyscallCall& call)
{
  if (isVirtualFD (call.args[0])) {
    call.id = -1;
    Socket::Ptr socket = getSocket (call.args[0]);
    if (socket) {
      int ret = socket->close();
      if (ret < 0)
        call.returnVal = -errno;
      else
        call.returnVal = ret;
    } else {
      call.returnVal = -EBADF;
    }
  }
}

int
Socket::close()
{
  return m_backend->close (localFD());
}

void
Sockets::do_listen(Sandbox::SyscallCall& call)
{
  if (isVirtualFD (call.args[0])) {
    call.id = -1;
    Socket::Ptr socket = getSocket (call.args[0]);
    if (socket) {
      call.returnVal = socket->listen (call.args[1]);
    } else {
      call.returnVal = -EBADF;
    }
  }
}

void
Sockets::do_accept(Sandbox::SyscallCall& call)
{
  if (isVirtualFD (call.args[0])) {
    call.id = -1;
    Socket::Ptr socket = getSocket (call.args[0]);
    if (socket) {
      call.returnVal = socket->accept (nullptr, 0);
    } else {
      call.returnVal = -EBADF;
    }
  }
}

void
Sockets::do_socket(Sandbox::SyscallCall& call)
{
  call.id = -1;
  call.returnVal = -EINVAL;
  for (auto i = m_backends.begin(); i != m_backends.end(); i++) {
    const SocketType& regType = i->first;
    SocketType newType (call.args[0], call.args[1], call.args[2]);
    if (regType.match (newType)) {
      SocketBackend::Ptr& backend = i->second;
      int newSocket = backend->socket (call.args[0], call.args[1], call.args[2]);
      if (newSocket) {
        Socket::Ptr socket (new Socket(newSocket, newType, backend));
        registerFD (socket);
        call.returnVal = socket->virtualFD();
        Debug() << "Got new socket";
      } else {
        call.returnVal = -errno;
      }
    }
  }
}

void
Sockets::do_setsockopt(Sandbox::SyscallCall& call)
{
  if (isVirtualFD (call.args[0])) {
    call.id = -1;
    Socket::Ptr socket = getSocket (call.args[0]);
    if (socket) {
      std::vector<char> val (call.args[4]);
      m_sbox->copyData (call.pid, call.args[3], val.size(), val.data());
      call.returnVal = socket->setsockopt(call.args[1], call.args[2], val.data(), val.size());
      if ((int)call.returnVal < 0)
        call.returnVal = -errno;
    } else {
      call.returnVal = -EBADF;
    }
  }
}

void
Sockets::do_bind(Sandbox::SyscallCall& call)
{
  if (isVirtualFD (call.args[0])) {
    call.id = -1;
    Socket::Ptr socket = getSocket (call.args[0]);
    if (socket) {
      std::vector<char> addrBuf (call.args[2]);
      m_sbox->copyData (call.pid, call.args[1], call.args[2], addrBuf.data());
      call.returnVal = socket->bind ((struct sockaddr*)addrBuf.data(), (socklen_t)addrBuf.size());
    } else {
      call.returnVal = -EBADF;
    }
  }
  /*struct sockaddr_un addr;
  addr.sun_family = AF_UNIX;
  snprintf (addr.sun_path, sizeof (addr.sun_path), "/tmp/codius-sandbox-socket-%d-%d", call.pid, static_cast<int>(call.args[0]));
  call.args[1] = m_sbox->writeScratch (sizeof (addr), reinterpret_cast<char*>(&addr));
  call.args[2] = sizeof (addr);*/
}

Socket::Socket(int localFD, const SocketType& type, std::shared_ptr<SocketBackend>& backend) :
  VirtualFD (localFD),
  m_type (type),
  m_backend (backend)
{
}

Socket::Ptr
Sockets::getSocket(int fd) const
{
  assert (isVirtualFD (fd));
  return std::static_pointer_cast<Socket> (getVirtualFD (fd));
}

int
Socket::bind(const struct sockaddr* addr, socklen_t addrlen)
{
  return m_backend->bind (localFD(), addr, addrlen);
}

int
Socket::listen(int backlog)
{
  return m_backend->listen (localFD(), backlog);
}

int
Socket::accept(struct sockaddr* addr, socklen_t* addrlen)
{
  return m_backend->accept (localFD(), addr, addrlen);
}

int
Socket::setsockopt(int level, int optname, const void* optval, socklen_t optlen)
{
  return m_backend->setsockopt (localFD(), level, optname, optval, optlen);
}
