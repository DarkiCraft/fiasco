#ifndef FIASCO_TCP_TRANSPORT_HPP
#define FIASCO_TCP_TRANSPORT_HPP

/// @file tcp_transport.hpp
/// @brief RAII wrapper around non-blocking POSIX TCP sockets.

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <stdexcept>
#include <string>

namespace fiasco {

/// @brief Sets a file descriptor to non-blocking mode.
/// @throws std::runtime_error on failure.
inline void set_nonblocking(int fd) {
  int flags = ::fcntl(fd, F_GETFL, 0);
  if (flags == -1) {
    throw std::runtime_error("fcntl F_GETFL failed: " +
                             std::string(std::strerror(errno)));
  }
  if (::fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
    throw std::runtime_error("fcntl F_SETFL O_NONBLOCK failed: " +
                             std::string(std::strerror(errno)));
  }
}

/// @brief RAII wrapper around a raw file descriptor (socket).
///
/// Movable, non-copyable. Closes the fd on destruction.
class socket_fd {
 public:
  /// Construct from a raw fd. Takes ownership.
  explicit socket_fd(int fd = -1) noexcept : m_fd(fd) {}

  ~socket_fd() { close(); }

  // Non-copyable
  socket_fd(const socket_fd&) = delete;
  socket_fd& operator=(const socket_fd&) = delete;

  // Movable
  socket_fd(socket_fd&& other) noexcept : m_fd(other.m_fd) { other.m_fd = -1; }

  socket_fd& operator=(socket_fd&& other) noexcept {
    if (this != &other) {
      close();
      m_fd = other.m_fd;
      other.m_fd = -1;
    }
    return *this;
  }

  /// @brief Returns the raw file descriptor.
  [[nodiscard]] int get() const noexcept { return m_fd; }

  /// @brief Returns true if the fd is valid (>= 0).
  [[nodiscard]] bool valid() const noexcept { return m_fd >= 0; }

  /// @brief Releases ownership and returns the raw fd.
  int release() noexcept {
    int fd = m_fd;
    m_fd = -1;
    return fd;
  }

  /// @brief Closes the fd if valid.
  void close() noexcept {
    if (m_fd >= 0) {
      ::close(m_fd);
      m_fd = -1;
    }
  }

 private:
  int m_fd;
};

/// @brief Non-blocking TCP transport.
///
/// Creates a server socket bound to the given address and port,
/// with SO_REUSEADDR and O_NONBLOCK set. This is the default
/// Transport template parameter for fiasco::server.
class tcp_transport {
 public:
  /// @brief Creates and binds a non-blocking server socket.
  /// @param host The address to bind to (e.g. "0.0.0.0").
  /// @param port The port to listen on.
  /// @param backlog The listen backlog (default 128).
  /// @throws std::runtime_error on any socket error.
  tcp_transport(const std::string& host, uint16_t port, int backlog = 128) {
    // Create socket
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
      throw std::runtime_error("socket() failed: " +
                               std::string(std::strerror(errno)));
    }
    m_server_fd = socket_fd(fd);

    // SO_REUSEADDR
    int opt = 1;
    if (::setsockopt(m_server_fd.get(), SOL_SOCKET, SO_REUSEADDR, &opt,
                     sizeof(opt)) < 0) {
      throw std::runtime_error("setsockopt SO_REUSEADDR failed: " +
                               std::string(std::strerror(errno)));
    }

    // Bind
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (::inet_pton(AF_INET, host.c_str(), &addr.sin_addr) <= 0) {
      throw std::runtime_error("inet_pton failed for host: " + host);
    }

    if (::bind(m_server_fd.get(), reinterpret_cast<struct sockaddr*>(&addr),
               sizeof(addr)) < 0) {
      throw std::runtime_error("bind() failed on " + host + ":" +
                               std::to_string(port) + ": " +
                               std::string(std::strerror(errno)));
    }

    // Listen
    if (::listen(m_server_fd.get(), backlog) < 0) {
      throw std::runtime_error("listen() failed: " +
                               std::string(std::strerror(errno)));
    }

    // Non-blocking
    set_nonblocking(m_server_fd.get());
  }

  /// @brief Returns the server socket fd for use with epoll.
  [[nodiscard]] int server_fd() const noexcept { return m_server_fd.get(); }

  /// @brief Accepts a new connection (non-blocking).
  /// @returns A socket_fd wrapping the client fd, or an invalid one if
  ///          EAGAIN/EWOULDBLOCK (no pending connections).
  /// @throws std::runtime_error on unexpected accept errors.
  [[nodiscard]] socket_fd accept() const {
    sockaddr_in client_addr{};
    socklen_t len = sizeof(client_addr);
    int client_fd =
        ::accept(m_server_fd.get(),
                 reinterpret_cast<struct sockaddr*>(&client_addr), &len);
    if (client_fd < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        return socket_fd{};  // No pending connection
      }
      throw std::runtime_error("accept() failed: " +
                               std::string(std::strerror(errno)));
    }

    set_nonblocking(client_fd);
    return socket_fd{client_fd};
  }

 private:
  socket_fd m_server_fd;
};

// -- Connection I/O helpers
// ----------------------------------------------------
//
// These free functions encapsulate all raw POSIX recv/send/close calls so
// that higher-level code (fiasco.hpp) stays free of direct syscalls.

/// @brief Result of a drain() call.
enum class drain_result {
  drained,   ///< EAGAIN reached — socket empty; more data may arrive later.
  closed,    ///< Peer sent FIN — connection closed cleanly.
  io_error,  ///< Unexpected recv() failure (errno set).
  feed_stopped,  ///< feed_fn returned false (parse error or request complete).
};

/// @brief Drains a non-blocking socket, feeding every chunk to feed_fn.
///
/// Reads in a tight loop until:
///   - EAGAIN / EWOULDBLOCK  -> returns drain_result::drained
///   - n == 0 (peer FIN)     -> returns drain_result::closed
///   - recv() error          -> returns drain_result::io_error
///   - feed_fn returns false -> returns drain_result::feed_stopped
///
/// This is the right companion for edge-triggered (EPOLLET) epoll: the
/// caller must drain until EAGAIN to avoid missing bytes.
///
/// @param fd      A non-blocking client socket fd.
/// @param feed_fn Callable with signature (const char*, std::size_t) -> bool.
///                Return false to stop reading early (parse error or done).
template <typename FeedFn>
drain_result drain(int fd, FeedFn&& feed_fn) {
  char buf[4096];
  while (true) {
    ssize_t n = ::recv(fd, buf, sizeof(buf), 0);
    if (n < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        return drain_result::drained;
      }
      return drain_result::io_error;
    }
    if (n == 0) {
      return drain_result::closed;
    }
    if (!feed_fn(buf, static_cast<std::size_t>(n))) {
      return drain_result::feed_stopped;
    }
  }
}

/// @brief Sends all bytes in data over fd. Retries on EINTR.
/// @returns true on success, false if send() fails permanently.
inline bool send_all(int fd, const std::string& data) noexcept {
  const char* ptr = data.c_str();
  std::size_t left = data.size();
  while (left > 0) {
    ssize_t sent = ::send(fd, ptr, left, MSG_NOSIGNAL);
    if (sent < 0) {
      if (errno == EINTR) {
        continue;
      }
      return false;
    }
    ptr += sent;
    left -= static_cast<std::size_t>(sent);
  }
  return true;
}

/// @brief Closes a file descriptor. No-op if fd < 0.
inline void close_fd(int fd) noexcept {
  if (fd >= 0) {
    ::close(fd);
  }
}

}  // namespace fiasco

#endif  // FIASCO_TCP_TRANSPORT_HPP
