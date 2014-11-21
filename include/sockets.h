#ifndef CODIUS_SOCKETS_H
#define CODIUS_SOCKETS_H

#include "sandbox.h"
#include "virtual-fd.h"

class SocketBackend;

struct SocketType {
  int domain;
  int type;
  int protocol;

  SocketType(int _domain, int _type, int _protocol) :
    domain (_domain),
    type (_type),
    protocol (_protocol) {}

  bool match(const SocketType& other) const
  {
    return (other.domain == -1 || domain == -1 || other.domain == domain) &&
           (other.type == -1 || type == -1 || other.type == type) &&
           (other.protocol == -1 || protocol == -1 || other.protocol == protocol);
  }
};

class Socket : public VirtualFD {
public:
  using Ptr = std::shared_ptr<Socket>;

  Socket(int localFD, const SocketType& type, std::shared_ptr<SocketBackend>& backend);

  int bind(const struct sockaddr* addr, socklen_t addrlen);
  int listen(int backlog);
  int accept(struct sockaddr* addr, socklen_t* addrlen);
  int close();
  int setsockopt(int level, int optname, const void* optval, socklen_t optlen);

private:
  const SocketType m_type;
  std::shared_ptr<SocketBackend> m_backend;
};

class SocketBackend {
public:
  using Ptr = std::shared_ptr<SocketBackend>;

  virtual int socket(int domain, int type, int protocol) = 0;
  virtual int bind(int sockfd, const struct sockaddr* addr, socklen_t addrlen) = 0;
  virtual int listen(int sockfd, int backlog) = 0;
  virtual int accept(int sockfd, struct sockaddr* addr, socklen_t* addrlen) = 0;
  virtual int close(int sockfd) = 0;
  virtual int setsockopt(int sockfd, int level, int optname, const void* optval, socklen_t oplen) = 0;
};

class Sockets : public VirtualFDGenerator {
public:

  using BackendRegistration = std::pair<SocketType, SocketBackend::Ptr>;

  Sockets(Sandbox* sandbox);
  Sandbox::SyscallCall handleSyscall(const Sandbox::SyscallCall& call);
  void registerBackend(SocketBackend::Ptr backend, int domain, int type, int protocol);

  Socket::Ptr getSocket(int fd) const;

private:
  void do_socket(Sandbox::SyscallCall& call);
  void do_bind(Sandbox::SyscallCall& call);
  void do_listen(Sandbox::SyscallCall& call);
  void do_accept(Sandbox::SyscallCall& call);
  void do_close(Sandbox::SyscallCall& call);
  void do_setsockopt(Sandbox::SyscallCall& call);

  Sandbox* m_sbox;
  std::vector<BackendRegistration> m_backends;
};

#endif // CODIUS_SOCKETS_H
