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
    HANDLE_CALL (getsockopt);
    HANDLE_CALL (read);
    HANDLE_CALL (connect);
    HANDLE_CALL (getsockname);
    HANDLE_CALL (write);
    HANDLE_CALL (sendmmsg);
    HANDLE_CALL (recvfrom);
    HANDLE_CALL (sendto);
  }
  return ret;
}

#undef HANDLE_CALL

void
Sockets::do_write(Sandbox::SyscallCall& call)
{
  Debug() << call.args[0];
  if (isVirtualFD (call.args[0])) {
    call.id = -1;
    Socket::Ptr socket = getSocket (call.args[0]);
    if (socket) {
      std::vector<char> buf (call.args[2]);
      m_sbox->copyData (call.pid, call.args[1], buf.size(), buf.data());
      call.returnVal = socket->write (buf.data(), buf.size());
    }
  }
}

void
Sockets::do_read(Sandbox::SyscallCall& call)
{
  if (isVirtualFD (call.args[0])) {
    call.id = -1;
    Socket::Ptr socket = getSocket (call.args[0]);
    if (socket) {
      std::vector<char> buf (call.args[2]);
      ssize_t readCount = socket->read (buf.data(), buf.size());
      if (readCount >= 0) {
        m_sbox->writeData (call.pid, call.args[1], readCount, buf.data());
      } else {
        call.returnVal = -errno;
      }
    }
  }
}

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
Sockets::do_connect(Sandbox::SyscallCall& call)
{
  if (isVirtualFD (call.args[0])) {
    call.id = -1;
    Socket::Ptr socket = getSocket (call.args[0]);
    if (socket) {
      std::vector<char> buf (call.args[2]);
      m_sbox->copyData (call.pid, call.args[1], buf.size(), buf.data());
      call.returnVal = socket->connect (reinterpret_cast<const struct sockaddr*>(buf.data()), buf.size());
    } else {
      call.returnVal = -EBADF;
    }
  }
}

void
Sockets::do_getsockname(Sandbox::SyscallCall& call)
{
  if (isVirtualFD (call.args[0])) {
    call.id = -1;
    Socket::Ptr socket = getSocket (call.args[0]);
    if (socket) {
      socklen_t sz;
      m_sbox->copyData (call.pid, call.args[2], sizeof (sz), &sz);
      std::vector<char> val (sz);
      call.returnVal = socket->getsockname (reinterpret_cast<struct sockaddr*> (val.data()), &sz);
      m_sbox->writeData (call.pid, call.args[2], sizeof (sz), reinterpret_cast<char*>(&sz));
      m_sbox->writeData (call.pid, call.args[1], sz, val.data());
    } else {
      call.returnVal = -EBADF;
    }
  }
}

void
Sockets::do_getsockopt(Sandbox::SyscallCall& call)
{
  Debug() << call.args[0];
  if (isVirtualFD (call.args[0])) {
    call.id = -1;
    Socket::Ptr socket = getSocket (call.args[0]);
    if (socket) {
      socklen_t sz;
      m_sbox->copyData (call.pid, call.args[4], sizeof (sz), &sz);
      std::vector<char> val (sz);
      call.returnVal = socket->getsockopt (call.args[1], call.args[2], val.data(), &sz);
      if (call.returnVal == 0) {
        m_sbox->writeData (call.pid, call.args[4], sizeof (sz), reinterpret_cast<char*>(&sz));
        m_sbox->writeData (call.pid, call.args[3], sz, val.data());
      }
    } else {
      call.returnVal = -EBADF;
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
Sockets::do_sendto(Sandbox::SyscallCall& call)
{
  if (isVirtualFD (call.args[0])) {
    call.id = -1;
    Socket::Ptr socket = getSocket (call.args[0]);
    if (socket) {
      std::vector<char> buf (call.args[2]);
      std::vector<char> addrBuf (call.args[5]);
      m_sbox->copyData (call.pid, call.args[4], addrBuf.size(), addrBuf.data());
      m_sbox->copyData (call.pid, call.args[1], buf.size(), buf.data());
      call.returnVal = socket->sendto (buf.data(), buf.size(), call.args[3], (struct sockaddr*)addrBuf.data(), addrBuf.size());
    } else {
      call.returnVal = -EBADF;
    }
  }
}

void
Sockets::do_sendmmsg(Sandbox::SyscallCall& call)
{
  Debug() << call.args[0];
  if (isVirtualFD (call.args[0])) {
    call.id = -1;
    Socket::Ptr socket = getSocket (call.args[0]);
    if (socket) {
      unsigned int vlen = call.args[2];
      unsigned int flags = call.args[3];
      mmsghdr buf[vlen];
      m_sbox->copyData (call.pid, call.args[1], sizeof (buf), buf);
      for (unsigned int i = 0; i < vlen; i++) {
        struct msghdr& hdr = buf[i].msg_hdr;
        std::vector<std::vector<char> > iovbufs (hdr.msg_iovlen);
        iovec iov[hdr.msg_iovlen];
        m_sbox->copyData (call.pid, reinterpret_cast<Sandbox::Address> (hdr.msg_iov), sizeof (iovec) * hdr.msg_iovlen, iov);
        hdr.msg_iov = iov;
        for (size_t j = 0; j < hdr.msg_iovlen; j++) {
          std::vector<char>& curBuf (iovbufs[j]);
          curBuf.resize (iov[j].iov_len);
          m_sbox->copyData (call.pid, reinterpret_cast<Sandbox::Address> (iov[j].iov_base), curBuf.size(), curBuf.data());
          iov[j].iov_base = curBuf.data();
        }
        int sent = socket->sendmsg (&hdr, flags);
        if (sent > -1) {
          m_sbox->writeData (call.pid, call.args[1] + sizeof (mmsghdr) * i + offsetof (mmsghdr, msg_len), sizeof (buf[0].msg_len), (char*)&sent);
        }
      }
    } else {
      call.returnVal = -EBADF;
    }
  }
}

void
Sockets::do_recvfrom(Sandbox::SyscallCall& call)
{
  if (isVirtualFD (call.args[0])) {
    call.id = -1;
    Socket::Ptr socket = getSocket (call.args[0]);
    if (socket) {
      std::vector<char> buf (call.args[2]);
      std::vector<char> addrBuf (call.args[5]);
      socklen_t sz;
      m_sbox->copyData (call.pid, call.args[5], sizeof (socklen_t), &sz);
      m_sbox->copyData (call.pid, call.args[4], addrBuf.size(), addrBuf.data());
      call.returnVal = socket->recvfrom (buf.data(), buf.size(), call.args[3], (struct sockaddr*)addrBuf.data(), &sz);
      call.args[5] = sz;
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

ssize_t
Socket::read(void* buf, size_t count)
{
  return m_backend->read (localFD(), buf, count);
}

int
Socket::connect(const struct sockaddr* addr, socklen_t addrlen)
{
  return m_backend->connect (localFD(), addr, addrlen);
}

int
Socket::getsockopt(int level, int optname, void* optval, socklen_t* optlen)
{
  return m_backend->getsockopt (localFD(), level, optname, optval, optlen);
}

int
Socket::getsockname(struct sockaddr* addr, socklen_t* addrlen)
{
  return m_backend->getsockname (localFD(), addr, addrlen);
}

ssize_t
Socket::write(const void* buf, size_t count)
{
  return m_backend->write (localFD(), buf, count);
}

int
Socket::sendmsg(const struct msghdr* msg, int flags)
{
  return m_backend->sendmsg (localFD(), msg, flags);
}

ssize_t
Socket::recvfrom(void* buf, size_t len, int flags, struct sockaddr* src_addr, socklen_t* addrlen)
{
  return m_backend->recvfrom (localFD(), buf, len, flags, src_addr, addrlen);
}

ssize_t
Socket::sendto(const void* buf, size_t len, int flags, const struct sockaddr* dest_addr, socklen_t addrlen)
{
  return m_backend->sendto (localFD(), buf, len, flags, dest_addr, addrlen);
}
