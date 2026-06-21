#include "fiasco/internal/core/tcp_server.hpp"

#include <asio.hpp>

#include <memory>

#include "fiasco/internal/http/parser.hpp"

namespace fiasco::detail {

using asio::ip::tcp;

struct session : std::enable_shared_from_this<session> {
    asio::strand<asio::io_context::executor_type> strand;
    tcp::socket socket;
    llhttp_parser parser;
    std::array<char, 8192> buf;
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

struct tcp_server::impl {
    asio::io_context m_ioc;
    request_handler m_handler;
    tcp::acceptor m_acceptor;
    asio::executor_work_guard<asio::io_context::executor_type> m_work_guard;
    unsigned int m_threads;
    asio::signal_set m_signals;

    impl(uint16_t port, const std::string& host, request_handler handler, unsigned int num_threads)
        : m_handler(std::move(handler)),
          m_acceptor(m_ioc, tcp::endpoint(asio::ip::make_address(host), port)),
          m_work_guard(asio::make_work_guard(m_ioc)),
          m_threads(num_threads == 0 ? std::max(std::thread::hardware_concurrency(), 2u)
                                     : num_threads),
          m_signals(m_ioc, SIGINT, SIGTERM) {
        m_acceptor.set_option(tcp::no_delay(true));

        m_signals.async_wait([this](asio::error_code ec, int) {
            if (!ec) {
                m_work_guard.reset();
                m_ioc.stop();
            }
        });
    }
};

tcp_server::tcp_server(uint16_t port,
                       const std::string& host,
                       request_handler handler,
                       unsigned int num_threads)
    : p_impl(std::make_unique<impl>(port, host, handler, num_threads)) {}

void tcp_server::run() {
    accept();
    std::vector<std::thread> threads;
    threads.reserve(p_impl->m_threads);
    for (unsigned int i = 0; i < p_impl->m_threads; ++i) {
        threads.emplace_back([this] { p_impl->m_ioc.run(); });
    }
    for (auto& t : threads) {
        t.join();
    }
}

void tcp_server::stop() noexcept {
    p_impl->m_work_guard.reset();
    p_impl->m_ioc.stop();
}

void tcp_server::accept() {
    p_impl->m_acceptor.async_accept(p_impl->m_ioc, [this](asio::error_code ec, tcp::socket socket) {
        if (!ec) {
            std::make_shared<session>(
                std::move(socket), p_impl->m_ioc.get_executor(), p_impl->m_handler)
                ->start();
        }
        if (p_impl->m_acceptor.is_open()) {
            accept();
        }
    });
}

tcp_server::~tcp_server() = default;

}  // namespace fiasco::detail