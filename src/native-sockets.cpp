#include "native-sockets.h"
#include <sys/socket.h>

#include "debug.h"

int
NativeSockets::socket(int domain, int type, int protocol)
{
  Debug() << domain << type << protocol;
  return ::socket (domain, type, protocol);
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
NativeSockets::setsockopt(int sockfd, int level, int optname, const void* optval, socklen_t optlen)
{
  Debug() << sockfd;
  return ::setsockopt (sockfd, level, optname, optval, optlen);
}
