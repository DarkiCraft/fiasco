#pragma once

#include <asio.hpp>
#include <cstdint>
#include <functional>
#include <thread>
#include <vector>

#include "fiasco/internal/http/parser.hpp"
#include "fiasco/internal/http/request.hpp"
#include "fiasco/internal/http/response.hpp"

namespace fiasco::detail {

using asio::ip::tcp;
using request_handler = std::function<response(request)>;

struct session : std::enable_shared_from_this<session> {
    // Strand ensures all completion handlers for this session run
    // serially — eliminates the data race on parser/handler across
    // the io_context thread pool with zero explicit locking.
    asio::strand<asio::io_context::executor_type> strand;
    tcp::socket socket;
    llhttp_parser parser;
    std::array<char, 8192> buf;  // 8 KiB: fits most request headers in one read
    const request_handler& handler;

    session(tcp::socket sock, asio::io_context::executor_type ex, const request_handler& h)
        : strand(asio::make_strand(ex)),
          socket(std::move(sock)),
          handler(h) {}

    void start() { read(); }

    void read() {
        auto self = shared_from_this();
        socket.async_read_some(
            asio::buffer(buf),
            asio::bind_executor(strand, [self](asio::error_code ec, std::size_t n) {
                if (ec) {
                    return;
                }
                self->parser.feed(self->buf.data(), n);
                if (self->parser.is_complete()) {
                    self->respond();
                } else {
                    self->read();
                }
            }));
    }

    void respond() {
        request req = parser.take_request();
        response res = handler(req);

        auto self = shared_from_this();
        auto raw = std::make_shared<std::string>(res.serialize());
        asio::async_write(
            socket,
            asio::buffer(*raw),
            asio::bind_executor(strand, [self, raw](asio::error_code ec, std::size_t) {
                if (!ec) {
                    self->parser.reset();  // reset parser state for next request
                    self->read();          // keep reading on same connection
                }
            }));
    }
};

class tcp_server {
  public:
    tcp_server(uint16_t port,
               const std::string& host,
               request_handler handler,
               unsigned int num_threads = 0)
        : m_handler(std::move(handler)),
          m_acceptor(m_ioc, tcp::endpoint(asio::ip::make_address(host), port)),
          m_work_guard(asio::make_work_guard(m_ioc)),
          m_threads(num_threads == 0 ? std::max(std::thread::hardware_concurrency(), 2u)
                                     : num_threads) {
        // Disable Nagle on every accepted socket via acceptor option.
        // TCP_NODELAY is essential for request/response latency —
        // Nagle batches small writes and adds up to 200ms delays.
        m_acceptor.set_option(tcp::no_delay(true));
    }

    void run() {
        accept();
        std::vector<std::thread> threads;
        threads.reserve(m_threads);
        for (unsigned int i = 0; i < m_threads; ++i) {
            threads.emplace_back([this] { m_ioc.run(); });
        }
        for (auto& t : threads) {
            t.join();
        }
    }

    void stop() noexcept {
        m_work_guard.reset();  // allow io_context::run() to drain and exit
        m_ioc.stop();
    }

  private:
    void accept() {
        // Pre-construct the socket so async_accept reuses it rather than
        // allocating a new one internally on each accept cycle.
        m_acceptor.async_accept(m_ioc, [this](asio::error_code ec, tcp::socket socket) {
            if (!ec) {
                std::make_shared<session>(std::move(socket), m_ioc.get_executor(), m_handler)
                    ->start();
            }
            // Only re-arm if the acceptor is still open; prevents a
            // tight infinite loop on persistent errors (e.g. EMFILE).
            if (m_acceptor.is_open()) {
                accept();
            }
        });
    }

    asio::io_context m_ioc;
    request_handler m_handler;  // declared before m_acceptor: constructed first
    tcp::acceptor m_acceptor;
    asio::executor_work_guard<asio::io_context::executor_type> m_work_guard;
    unsigned int m_threads;
};

}  // namespace fiasco::detail