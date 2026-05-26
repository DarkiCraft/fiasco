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
#include "fiasco/core/connection.hpp"
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
    add(detail::http_method::get, path, std::forward<F>(f));
  }

  template <typename F>
  void post(const std::string& path, F&& f) {
    add(detail::http_method::post, path, std::forward<F>(f));
  }

  template <typename F>
  void put(const std::string& path, F&& f) {
    add(detail::http_method::put, path, std::forward<F>(f));
  }

  template <typename F>
  void del(const std::string& path, F&& f) {
    add(detail::http_method::del, path, std::forward<F>(f));
  }

  template <typename F>
  void patch(const std::string& path, F&& f) {
    add(detail::http_method::patch, path, std::forward<F>(f));
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
  detail::response dispatch(detail::request req) {
    auto match = m_router.match(req.method, req.path);

    if (!match.matched) {
      if (m_router.any_method_matches(req.path))
        return detail::response::to_error("Method Not Allowed", 405);
      return detail::response::to_error("Not Found", 404);
    }

    req.path_params = std::move(match.path_params);
    req.ordered_path_params = std::move(match.ordered_path_params);

    try {
      return match.handler(std::move(req));
    } catch (const nlohmann::json::exception& e) {
      return detail::response::to_error(e.what(), 422);
    } catch (const std::exception& e) {
      return detail::response::to_error(e.what(), 500);
    } catch (...) {
      return detail::response::to_error("Internal Server Error", 500);
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
    detail::tcp_transport transport(host, port);
    detail::event_loop loop;

    loop.run(transport, [&](int client_fd, detail::event_loop& lp) {
      // -- Read phase (epoll thread) -------------------------------------
      detail::connection& conn = get_or_create_connection(client_fd);

      auto result = conn.read();

      if (result == detail::connection::ReadResult::error) {
        close_connection(lp, client_fd);
        return;
      }

      if (result == detail::connection::ReadResult::partial) {
        return;  // Stay in epoll; wait for more data.
      }

      // -- Full request received — hand off to thread pool ---------------
      detail::request req = conn.take_request();

      // Determine keep-alive preference from the request header.
      const std::string conn_hdr = req.header("Connection");
      conn.keep_alive = (conn_hdr != "close" && conn_hdr != "Close");

      // Remove from epoll before submitting: prevents a second event from
      // racing with the pool worker that now owns this fd.
      lp.remove_fd(client_fd);

      bool queued = m_pool.try_submit(
          [this, &lp, client_fd, req = std::move(req)]() mutable {
            // -- Dispatch phase (pool worker) ----------------------------
            detail::response res = dispatch(std::move(req));

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
        detail::connection& c = get_connection(client_fd);
        c.write(detail::response::to_error("Server Too Busy", 503));
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
  detail::di_container m_di;
  detail::thread_pool m_pool;

  /// Per-connection state, keyed by fd.
  /// Accessed from both the epoll thread and pool workers — always hold
  /// conns_mtx_ when reading or writing this map.
  std::unordered_map<int, detail::connection> m_conns;
  mutable std::array<std::mutex, 16> m_conns_mtx;

  // -- Private helpers -------------------------------------------------------

  /// @brief Returns the mutex shard for a given file descriptor.
  std::mutex& shard(int fd) { return m_conns_mtx[fd % 16]; }

  /// @brief Returns (or constructs) the connection for a given fd.
  detail::connection& get_or_create_connection(int fd) {
    std::lock_guard lk(shard(fd));
    auto [it, _] =
        m_conns.emplace(std::piecewise_construct, std::forward_as_tuple(fd),
                        std::forward_as_tuple(fd));
    return it->second;
  }

  /// @brief Returns the connection for a given fd. UB if fd not present.
  detail::connection& get_connection(int fd) {
    std::lock_guard lk(shard(fd));
    return m_conns.at(fd);
  }

  /// @brief Removes the fd from epoll, closes it, and erases the connection.
  void close_connection(detail::event_loop& lp, int fd) {
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
  void add(detail::http_method method, const std::string& pattern, F&& f) {
    m_router.add_route(method, pattern, make_handler(std::forward<F>(f), m_di));
  }
};

}  // namespace fiasco

#endif  // FIASCO_FIASCO_HPP
