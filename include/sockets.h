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
  int connect(const struct sockaddr* addr, socklen_t addrlen);
  int getsockopt(int level, int optname, void* optval, socklen_t* optlen);
  ssize_t read(void* buf, size_t count);
  int getsockname(struct sockaddr* addr, socklen_t* addrlen);
  ssize_t write(const void* buf, size_t count);
  int sendmsg(const struct msghdr* msg, int flags);
  ssize_t recvfrom(void* buf, size_t len, int flags, struct sockaddr* src_addr, socklen_t* addrlen);
  ssize_t sendto(const void* buf, size_t len, int flags, const struct sockaddr* dest_addr, socklen_t addrlen);

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
  virtual int getsockopt(int sockfd, int level, int optname, void* optval, socklen_t* optlen) = 0;
  virtual ssize_t read(int sockfd, void* buf, size_t count) = 0;
  virtual int connect(int sockfd, const struct sockaddr* addr, socklen_t addrlen) = 0;
  virtual int getsockname(int sockfd, struct sockaddr* addr, socklen_t* addrlen) = 0;
  virtual ssize_t write(int sockfd, const void* buf, size_t count) = 0;
  virtual ssize_t sendmsg(int sockfd, const struct msghdr* msg, int flags) = 0;
  virtual ssize_t recvfrom(int sockfd, void* buf, size_t len, int flags, struct sockaddr* src_addr, socklen_t* addrlen) = 0;
  virtual ssize_t sendto(int sockfd, const void* buf, size_t len, int flags, const struct sockaddr* dest_addr, socklen_t addrlen) = 0;
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
  void do_read(Sandbox::SyscallCall& call);
  void do_getsockopt(Sandbox::SyscallCall& call);
  void do_connect(Sandbox::SyscallCall& call);
  void do_getsockname(Sandbox::SyscallCall& call);
  void do_write(Sandbox::SyscallCall& call);
  void do_sendmmsg(Sandbox::SyscallCall& call);
  void do_recvfrom(Sandbox::SyscallCall& call);
  void do_sendto(Sandbox::SyscallCall& call);

  Sandbox* m_sbox;
  std::vector<BackendRegistration> m_backends;
};

#endif // CODIUS_SOCKETS_H
