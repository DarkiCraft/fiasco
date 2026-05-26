#ifndef FIASCO_EVENT_LOOP_HPP
#define FIASCO_EVENT_LOOP_HPP

/// @file event_loop.hpp
/// @brief Non-blocking epoll event loop.

#include <sys/epoll.h>
#include <unistd.h>

#include <cstring>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

#include "fiasco/core/tcp_transport.hpp"

namespace fiasco {
/// @brief Callback signature for readable client connections.
///
/// The callback receives the client fd and a reference to the event_loop
/// so it can modify/remove the fd from epoll if needed.
class event_loop;
using on_read_callback = std::function<void(int client_fd, event_loop& loop)>;

/// @brief Non-blocking epoll event loop.
///
/// Manages the lifecycle of an epoll instance. Monitors a server socket
/// for new connections and client sockets for readable data.
class event_loop {
 public:
  /// @brief Creates an epoll instance.
  /// @param max_events Max events to process per epoll_wait call.
  /// @throws std::runtime_error on epoll_create1 failure.
  explicit event_loop(int max_events = 1024)
      : m_max_events(max_events), m_events(max_events) {
    m_epoll_fd = socket_fd(::epoll_create1(0));
    if (!m_epoll_fd.valid()) {
      throw std::runtime_error("epoll_create1 failed: " +
                               std::string(std::strerror(errno)));
    }
  }

  /// @brief Registers a file descriptor for EPOLLIN | EPOLLET (edge-triggered).
  /// @throws std::runtime_error on epoll_ctl failure.
  void add_fd(int fd) {
    ::epoll_event ev{};
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = fd;
    if (::epoll_ctl(m_epoll_fd.get(), EPOLL_CTL_ADD, fd, &ev) < 0) {
      throw std::runtime_error("epoll_ctl ADD failed for fd " +
                               std::to_string(fd) + ": " +
                               std::string(std::strerror(errno)));
    }
  }

  /// @brief Removes a file descriptor from epoll.
  void remove_fd(int fd) noexcept {
    ::epoll_ctl(m_epoll_fd.get(), EPOLL_CTL_DEL, fd, nullptr);
  }

  /// @brief Runs the event loop.
  ///
  /// Monitors the server socket for new connections and calls
  /// on_read for each client socket that has data ready.
  ///
  /// @param transport The tcp_transport providing the server socket.
  /// @param on_read   Callback invoked when a client fd is readable.
  void run(const tcp_transport& transport, const on_read_callback& on_read) {
    add_fd(transport.server_fd());
    m_running = true;

    while (m_running) {
      int n = ::epoll_wait(m_epoll_fd.get(), m_events.data(), m_max_events,
                           100 /* timeout ms — allows periodic stop check */);
      if (n < 0) {
        if (errno == EINTR) {
          continue;  // Interrupted by signal, retry
        }
        throw std::runtime_error("epoll_wait failed: " +
                                 std::string(std::strerror(errno)));
      }

      for (int i = 0; i < n; ++i) {
        int fd = m_events[i].data.fd;

        if (fd == transport.server_fd()) {
          // Accept all pending connections (edge-triggered)
          accept_connections(transport);
        } else if (m_events[i].events & EPOLLIN) {
          on_read(fd, *this);
        }

        // Handle errors / hangups
        if (m_events[i].events & (EPOLLERR | EPOLLHUP)) {
          remove_fd(fd);
          ::close(fd);
        }
      }
    }
  }

  /// @brief Signals the event loop to stop after the current iteration.
  void stop() noexcept { m_running = false; }

  /// @brief Returns whether the loop is running.
  [[nodiscard]] bool is_running() const noexcept { return m_running; }

 private:
  /// @brief Accepts all pending connections from the server socket.
  void accept_connections(const tcp_transport& transport) {
    while (true) {
      auto client = transport.accept();
      if (!client.valid()) {
        break;  // EAGAIN — no more pending connections
      }
      int client_fd = client.release();  // We manage lifetime via epoll
      add_fd(client_fd);
    }
  }

  socket_fd m_epoll_fd;
  int m_max_events;
  std::vector<epoll_event> m_events;
  volatile bool m_running = false;
};

}  // namespace fiasco

#endif  // FIASCO_EVENT_LOOP_HPP
