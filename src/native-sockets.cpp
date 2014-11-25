#include "native-sockets.h"
#include <sys/socket.h>

#include "debug.h"

int
NativeSockets::socket(int domain, int type, int protocol)
{
  Debug() << domain << type << protocol;
  int ret = ::socket (domain, type, protocol);
  if (ret < 0)
    return -errno;
  return ret;
}

int
NativeSockets::bind(int sockfd, const struct sockaddr* addr, socklen_t addrlen)
{
  Debug() << sockfd;
  return ::bind (sockfd, addr, addrlen);
}

int
NativeSockets::listen(int sockfd, int backlog)
{
  Debug() << sockfd;
  return ::listen (sockfd, backlog);
}

int
NativeSockets::accept(int sockfd, struct sockaddr* addr, socklen_t* addrlen)
{
  Debug() << sockfd;
  return ::accept (sockfd, addr, addrlen);
}

int
NativeSockets::close(int sockfd)
{
  Debug() << sockfd;
  return ::close (sockfd);
}

int
NativeSockets::getsockopt(int sockfd, int level, int optname, void* optval, socklen_t* optlen)
{
  Debug() << sockfd;
  ::getsockopt (sockfd, level, optname, optval, optlen);
  return -errno;
}

int
NativeSockets::setsockopt(int sockfd, int level, int optname, const void* optval, socklen_t optlen)
{
  Debug() << sockfd;
  return ::setsockopt (sockfd, level, optname, optval, optlen);
}

ssize_t
NativeSockets::read(int sockfd, void* buf, size_t count)
{
  Debug() << sockfd;
  return ::read (sockfd, buf, count);
}

int
NativeSockets::connect(int sockfd, const struct sockaddr* addr, socklen_t addrlen)
{
  Debug() << sockfd;
  ::connect (sockfd, addr, addrlen);
  return -errno;
}

int
NativeSockets::getsockname(int sockfd, struct sockaddr* addr, socklen_t* addrlen)
{
  Debug() << sockfd;
  return ::getsockname (sockfd, addr, addrlen);
}

ssize_t
NativeSockets::write(int sockfd, const void* buf, size_t count)
{
  Debug() << sockfd;
  return ::write (sockfd, buf, count);
}

ssize_t
NativeSockets::sendmsg(int sockfd, const struct msghdr* msg, int flags)
{
  Debug() << sockfd;
  ::sendmsg (sockfd, msg, flags);
  return -errno;
}

ssize_t
NativeSockets::recvfrom(int sockfd, void* buf, size_t len, int flags, struct sockaddr* src_addr, socklen_t* addrlen)
{
  Debug() << sockfd;
  return ::recvfrom (sockfd, buf, len, flags, src_addr, addrlen);
}

ssize_t
NativeSockets::sendto(int sockfd, const void* buf, size_t len, int flags, const struct sockaddr* dest_addr, socklen_t addrlen)
{
  Debug() << sockfd;
  return ::sendto (sockfd, buf, len, flags, dest_addr, addrlen);
}
