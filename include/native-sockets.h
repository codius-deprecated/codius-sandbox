#ifndef CODIUS_NATIVE_SOCKETS_H
#define CODIUS_NATIVE_SOCKETS_H

#include "sockets.h"

class NativeSockets : public SocketBackend {
public:
  int socket(int domain, int type, int protocol) override;
  int bind(int sockfd, const struct sockaddr* addr, socklen_t addrlen) override;
  int listen(int sockfd, int backlog) override;
  int accept(int sockfd, struct sockaddr* addr, socklen_t* addrlen) override;
  int close(int sockfd) override;
  int setsockopt(int sockfd, int level, int optname, const void* optval, socklen_t optlen) override;
  int getsockopt(int sockfd, int level, int optname, void* optval, socklen_t* optlen) override;
  ssize_t read(int sockfd, void* buf, size_t count) override;
  int connect(int sockfd, const struct sockaddr* addr, socklen_t addrlen) override;
  int getsockname(int sockfd, struct sockaddr* addr, socklen_t* addrlen) override;
  ssize_t write(int sockfd, const void* buf, size_t count) override;
  ssize_t sendmsg(int sockfd, const struct msghdr* msg, int flags) override;
  ssize_t recvfrom(int sockfd, void* buf, size_t len, int flags, struct sockaddr* src_addr, socklen_t* addrlen) override;
  ssize_t sendto(int sockfd, const void* buf, size_t len, int flags, const struct sockaddr* dest_addr, socklen_t addrlen) override;
};

#endif // CODIUS_NATIVE_SOCKETS_H
