#ifndef FIASCO_FIASCO_HPP
#define FIASCO_FIASCO_HPP

/// @file fiasco.hpp
/// @brief Main public header for the fiasco web framework.
///
/// Single include for framework users. Orchestrates the pipeline:
///
///   epoll event
///      -> connection::read()      (drain socket, feed parser)
///      -> connection::complete()  (check if a full request is ready)
///      -> thread_pool::submit()
///              -> server::dispatch()   (route -> handler -> response)
///              -> connection::write()  (send serialized response)
///              -> keep-alive / close
///
/// No raw POSIX calls are made here. All I/O is delegated to
/// tcp_transport (connection helpers) and the event_loop.

#include <mutex>
#include <thread>
#include <unordered_map>

// -- Core --------------------------------------------------------------------
#include "fiasco/core/event_loop.hpp"
#include "fiasco/core/tcp_transport.hpp"
#include "fiasco/core/thread_pool.hpp"

// -- HTTP --------------------------------------------------------------------
#include "fiasco/http/parser.hpp"
#include "fiasco/http/request.hpp"
#include "fiasco/http/response.hpp"

// -- Serialization -----------------------------------------------------------
#include "fiasco/serialization/model.hpp"

// -- Routing -----------------------------------------------------------------
#include "fiasco/routing/function_traits.hpp"
#include "fiasco/routing/router.hpp"

namespace fiasco {

/// @brief Returns the library version string.
inline const char* version() noexcept { return "0.1.0"; }

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

    auto dr = drain(
        fd, [this, &parse_error](const char* buf, std::size_t len) -> bool {
          if (!parser.feed(buf, len)) {
            parse_error = true;
            return false;  // stop — bad HTTP
          }
          return !parser.is_complete();  // stop early once a full request is
                                         // ready
        });

    if (dr == drain_result::io_error || parse_error) return ReadResult::error;
    if (dr == drain_result::closed && !parser.is_complete())
      return ReadResult::error;
    if (parser.is_complete()) return ReadResult::complete;
    return ReadResult::partial;
  }

  /// @brief Moves the completed request out of the parser (resets for reuse).
  [[nodiscard]] request take_request() { return parser.take_request(); }

  /// @brief Serializes and sends the response over this connection.
  void write(const response& res) {
    auto raw = res.serialize();
    send_all(fd, raw);  // free function from tcp_transport.hpp
  }

  /// @brief Closes the underlying fd.
  void close() { close_fd(fd); }  // free function from tcp_transport.hpp
};

// -- Server -------------------------------------------------------------------

class server {
 public:
  // -- Constructor ---------------------------------------------------------

  /// @brief Creates a server with optional tuning parameters.
  ///
  /// @param num_threads     Worker threads for handler dispatch.
  ///                        Defaults to hardware concurrency (minimum 2).
  /// @param max_queue_depth Maximum pending tasks in the thread pool queue.
  ///                        0 = unbounded. When the limit is reached, the
  ///                        server responds 503 instead of queuing.
  explicit server(unsigned int num_threads = 0, std::size_t max_queue_depth = 0)
      : m_pool(num_threads == 0
                   ? std::max(std::thread::hardware_concurrency(), 2u)
                   : num_threads,
               max_queue_depth) {}

  // -- Route registration --------------------------------------------------

  template <typename F>
  void get(const std::string& path, F&& f) {
    add(http_method::get, path, std::forward<F>(f));
  }

  template <typename F>
  void post(const std::string& path, F&& f) {
    add(http_method::post, path, std::forward<F>(f));
  }

  template <typename F>
  void put(const std::string& path, F&& f) {
    add(http_method::put, path, std::forward<F>(f));
  }

  template <typename F>
  void del(const std::string& path, F&& f) {
    add(http_method::del, path, std::forward<F>(f));
  }

  template <typename F>
  void patch(const std::string& path, F&& f) {
    add(http_method::patch, path, std::forward<F>(f));
  }

  // -- Dependency injection -------------------------------------------------

  /// @brief Registers a singleton factory for type T.
  ///
  /// The factory is invoked once on first use; the result is cached forever.
  ///
  /// @code
  ///   app.provide<db_session>([]{ return db_session::connect("…"); });
  /// @endcode
  template <typename T, typename Factory>
  void provide(Factory&& factory) {
    m_di.provide<T>(std::forward<Factory>(factory));
  }

  // -- Dispatch -------------------------------------------------------------

  /// @brief Routes a parsed request to the matching handler and returns the
  ///        response. All exceptions are caught and mapped to HTTP error codes.
  response dispatch(request req) {
    auto match = m_router.match(req.method, req.path);

    if (!match.matched) {
      if (m_router.any_method_matches(req.path))
        return response::to_error("Method Not Allowed", 405);
      return response::to_error("Not Found", 404);
    }

    req.path_params = std::move(match.path_params);
    req.ordered_path_params = std::move(match.ordered_path_params);

    try {
      return match.handler(std::move(req));
    } catch (const nlohmann::json::exception& e) {
      return response::to_error(e.what(), 422);
    } catch (const std::exception& e) {
      return response::to_error(e.what(), 500);
    } catch (...) {
      return response::to_error("Internal Server Error", 500);
    }
  }

  // -- Run ------------------------------------------------------------------

  /// @brief Binds to host:port and enters the epoll event loop. Blocks until
  ///        stop() is called.
  ///
  /// ## Request pipeline (per event)
  ///
  ///   epoll thread  — owns the connection map; calls conn.read() to drain
  ///                   the socket and advance the HTTP parser. Never touches
  ///                   handler code, never blocks on I/O.
  ///
  ///   pool worker   — calls dispatch(), writes the response, then either
  ///                   re-arms the fd for keep-alive or closes it.
  ///
  /// The fd is removed from epoll *before* the task is submitted so no
  /// concurrent epoll event can fire while the pool worker owns the fd.
  void run(uint16_t port = 8080, const std::string& host = "0.0.0.0") {
    tcp_transport transport(host, port);
    event_loop loop;

    loop.run(transport, [&](int client_fd, event_loop& lp) {
      // -- Read phase (epoll thread) -------------------------------------
      connection& conn = get_or_create_connection(client_fd);

      auto result = conn.read();

      if (result == connection::ReadResult::error) {
        close_connection(lp, client_fd);
        return;
      }

      if (result == connection::ReadResult::partial) {
        return;  // Stay in epoll; wait for more data.
      }

      // -- Full request received — hand off to thread pool ---------------
      request req = conn.take_request();

      // Determine keep-alive preference from the request header.
      const std::string conn_hdr = req.header("Connection");
      conn.keep_alive = (conn_hdr != "close" && conn_hdr != "Close");

      // Remove from epoll before submitting: prevents a second event from
      // racing with the pool worker that now owns this fd.
      lp.remove_fd(client_fd);

      bool queued = m_pool.try_submit(
          [this, &lp, client_fd, req = std::move(req)]() mutable {
            // -- Dispatch phase (pool worker) ----------------------------
            response res = dispatch(std::move(req));

            // Reflect the keep-alive decision in the response header.
            bool keep_alive;
            {
              std::lock_guard<std::mutex> lk(shard(client_fd));
              auto it = m_conns.find(client_fd);
              keep_alive = (it != m_conns.end()) && it->second.keep_alive;
            }

            res.headers["Connection"] = keep_alive ? "keep-alive" : "close";

            // Write response and manage connection lifetime.
            {
              std::lock_guard<std::mutex> lk(shard(client_fd));
              auto it = m_conns.find(client_fd);
              if (it == m_conns.end()) return;  // Already gone.
              it->second.write(res);
            }

            if (keep_alive) {
              lp.add_fd(client_fd);  // Re-arm for next request.
            } else {
              close_connection(lp, client_fd);
            }
          });

      if (!queued) {
        // Pool queue full — send 503 and close immediately.
        connection& c = get_connection(client_fd);
        c.write(response::to_error("Server Too Busy", 503));
        close_connection(lp, client_fd);
      }
    });
  }

  /// @brief Signals the event loop to stop after the current iteration.
  void stop() { /* Stored loop reference or flag would go here — future work */
  }

 private:
  // -- Members --------------------------------------------------------------

  router m_router;
  di_container m_di;
  thread_pool m_pool;

  /// Per-connection state, keyed by fd.
  /// Accessed from both the epoll thread and pool workers — always hold
  /// conns_mtx_ when reading or writing this map.
  std::unordered_map<int, connection> m_conns;
  mutable std::array<std::mutex, 16> m_conns_mtx;

  // -- Private helpers -------------------------------------------------------

  /// @brief Returns the mutex shard for a given file descriptor.
  std::mutex& shard(int fd) { return m_conns_mtx[fd % 16]; }

  /// @brief Returns (or constructs) the connection for a given fd.
  connection& get_or_create_connection(int fd) {
    std::lock_guard lk(shard(fd));
    auto [it, _] =
        m_conns.emplace(std::piecewise_construct, std::forward_as_tuple(fd),
                        std::forward_as_tuple(fd));
    return it->second;
  }

  /// @brief Returns the connection for a given fd. UB if fd not present.
  connection& get_connection(int fd) {
    std::lock_guard lk(shard(fd));
    return m_conns.at(fd);
  }

  /// @brief Removes the fd from epoll, closes it, and erases the connection.
  void close_connection(event_loop& lp, int fd) {
    lp.remove_fd(fd);
    std::lock_guard lk(shard(fd));
    auto it = m_conns.find(fd);
    if (it != m_conns.end()) {
      it->second.close();
      m_conns.erase(it);
    }
  }

  /// @brief Wraps a user callable into a normalized handler_fn and registers
  ///        it on the router.
  template <typename F>
  void add(http_method method, const std::string& pattern, F&& f) {
    m_router.add_route(method, pattern, make_handler(std::forward<F>(f), m_di));
  }
};

}  // namespace fiasco

#endif  // FIASCO_FIASCO_HPP
