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
};

#endif // CODIUS_NATIVE_SOCKETS_H
