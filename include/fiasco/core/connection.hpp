#ifndef FIASCO_CORE_CONNECTION_HPP
#define FIASCO_CORE_CONNECTION_HPP

/// @file connection.hpp
/// @brief connection type to handle epoll connections

#include "fiasco/core/tcp_transport.hpp"
#include "fiasco/http/parser.hpp"
#include "fiasco/http/response.hpp"

namespace fiasco::detail {

// -- Connection ---------------------------------------------------------------

/// @brief Per-connection state: owns the fd, its HTTP parser, and keep-alive
///        preference.
///
/// Lifetime: created on the epoll thread when new data arrives for a client
/// fd, destroyed after the response is sent (or on error). The server holds
/// these in an fd-keyed map guarded by a mutex.
///
/// All I/O (read / write / close) is delegated to tcp_transport free functions
/// so that fiasco.hpp stays free of raw POSIX calls.
struct connection {
  int fd;
  llhttp_parser parser;
  bool keep_alive = true;

  explicit connection(int fd) : fd(fd) {}

  // Non-copyable — parsers are not cheap to copy and fds have identity.
  connection(const connection&) = delete;
  connection& operator=(const connection&) = delete;

  connection(connection&&) = default;
  connection& operator=(connection&&) = default;

  /// @brief Drains the socket and feeds bytes into the HTTP parser.
  ///
  /// @returns ReadResult signalling what happened:
  ///   - complete  : a full request was parsed; call take_request().
  ///   - partial   : need more data; stay in epoll.
  ///   - error     : I/O error or bad HTTP; caller should close.
  enum class ReadResult { complete, partial, error };

  ReadResult read() {
    bool parse_error = false;

    auto dr = detail::drain(
        fd, [this, &parse_error](const char* buf, std::size_t len) {
          if (!parser.feed(buf, len)) {
            parse_error = true;
            return false;  // stop — bad HTTP
          }
          return !parser.is_complete();  // stop early once a full request is
                                         // ready
        });

    if (dr == detail::drain_result::io_error || parse_error)
      return ReadResult::error;
    if (dr == detail::drain_result::closed && !parser.is_complete())
      return ReadResult::error;
    if (parser.is_complete()) return ReadResult::complete;
    return ReadResult::partial;
  }

  /// @brief Moves the completed request out of the parser (resets for reuse).
  [[nodiscard]] detail::request take_request() { return parser.take_request(); }

  /// @brief Serializes and sends the response over this connection.
  void write(const detail::response& res) {
    auto raw = res.serialize();
    detail::send_all(fd, raw);  // free function from tcp_transport.hpp
  }

  /// @brief Closes the underlying fd.
  void close() {
    detail::close_fd(fd);
  }  // free function from tcp_transport.hpp
};

}  // namespace fiasco::detail

#endif  // FIASCO_CORE_CONNECTION_HPP