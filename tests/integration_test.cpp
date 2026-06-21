#include <catch2/catch_test_macros.hpp>

#include <array>
#include <chrono>
#include <string>
#include <thread>

#include <asio.hpp>

#include "fiasco/fiasco.hpp"

using asio::ip::tcp;

// -------------------------------------------------------------------
// Helpers
// -------------------------------------------------------------------

static uint16_t find_free_port() {
    asio::io_context ioc;
    tcp::acceptor acceptor(ioc, tcp::endpoint(tcp::v4(), 0));
    uint16_t port = acceptor.local_endpoint().port();
    acceptor.close();
    return port;
}

static void wait_for_server(uint16_t port) {
    for (int i = 0; i < 50; ++i) {
        try {
            asio::io_context ioc;
            tcp::socket socket(ioc);
            socket.connect(tcp::endpoint(asio::ip::address_v4::loopback(), port));
            socket.close();
            return;
        } catch (...) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    }
}

// RAII guard: stops the server and joins the thread on destruction,
// even when a REQUIRE failure unwinds the stack.
struct ServerGuard {
    fiasco::server& app;
    std::thread& t;
    ~ServerGuard() {
        app.stop();
        if (t.joinable()) {
            t.join();
        }
    }
};

static std::string send_and_receive(uint16_t port, const std::string& raw) {
    asio::io_context ioc;
    tcp::socket socket(ioc);
    socket.connect(tcp::endpoint(asio::ip::address_v4::loopback(), port));

    asio::write(socket, asio::buffer(raw));

    std::string response;
    std::array<char, 4096> buf;
    asio::error_code ec;

    while (response.find("\r\n\r\n") == std::string::npos) {
        size_t n = socket.read_some(asio::buffer(buf), ec);
        if (ec) {
            break;
        }
        response.append(buf.data(), n);
    }

    // Read the rest of the body based on Content-Length
    auto header_end = response.find("\r\n\r\n");
    auto headers = response.substr(0, header_end);
    size_t content_length = 0;
    auto cl = headers.find("Content-Length: ");
    if (cl != std::string::npos) {
        auto start = cl + 16;
        auto end = headers.find("\r\n", start);
        content_length = std::stoul(headers.substr(start, end - start));
    }

    while (response.size() < header_end + 4 + content_length) {
        size_t n = socket.read_some(asio::buffer(buf), ec);
        if (ec) {
            break;
        }
        response.append(buf.data(), n);
    }

    return response;
}

static int extract_status(const std::string& response) {
    auto s1 = response.find(' ');
    auto s2 = response.find(' ', s1 + 1);
    return std::stoi(response.substr(s1 + 1, s2 - s1 - 1));
}

static std::string extract_body(const std::string& response) {
    auto hd = response.find("\r\n\r\n");
    if (hd == std::string::npos) {
        return "";
    }
    return response.substr(hd + 4);
}

// -------------------------------------------------------------------
// Helpers: struct used in body-deserialization tests
// Struct + FIASCO_MODEL must be inside fiasco::detail for ADL.
// -------------------------------------------------------------------
namespace fiasco::detail {

struct SumInput {
    int a = 0;
    int b = 0;
};

FIASCO_MODEL(SumInput, a, b)

}  // namespace fiasco::detail

// -------------------------------------------------------------------
// Tests
// -------------------------------------------------------------------

TEST_CASE("integration GET returns 200", "[integration]") {
    fiasco::server app;
    app.get("/hello", []() -> std::string { return "world"; });

    auto port = find_free_port();
    std::thread t([&] { app.run(port, "127.0.0.1"); });
    wait_for_server(port);
    ServerGuard guard{app, t};

    auto resp = send_and_receive(port, "GET /hello HTTP/1.1\r\nHost: localhost\r\n\r\n");
    REQUIRE(extract_status(resp) == 200);
    REQUIRE(extract_body(resp) == "world\n");
}

TEST_CASE("integration 404 not found", "[integration]") {
    fiasco::server app;
    app.get("/exists", []() -> std::string { return "ok"; });

    auto port = find_free_port();
    std::thread t([&] { app.run(port, "127.0.0.1"); });
    wait_for_server(port);
    ServerGuard guard{app, t};

    auto resp = send_and_receive(port, "GET /nonexistent HTTP/1.1\r\nHost: localhost\r\n\r\n");
    REQUIRE(extract_status(resp) == 404);
    REQUIRE(extract_body(resp).find("Not Found") != std::string::npos);
}

TEST_CASE("integration 405 method not allowed", "[integration]") {
    fiasco::server app;
    app.get("/resource", []() -> std::string { return "ok"; });

    auto port = find_free_port();
    std::thread t([&] { app.run(port, "127.0.0.1"); });
    wait_for_server(port);
    ServerGuard guard{app, t};

    auto resp = send_and_receive(
        port, "POST /resource HTTP/1.1\r\nHost: localhost\r\nContent-Length: 0\r\n\r\n");
    REQUIRE(extract_status(resp) == 405);
}

TEST_CASE("integration invalid JSON body returns 500", "[integration]") {
    // resolve_arg converts json::exception to std::runtime_error,
    // so dispatch's 422 catch is shadowed → 500.
    fiasco::server app;
    app.post("/sum", [](const fiasco::detail::SumInput&) -> std::string { return ""; });

    auto port = find_free_port();
    std::thread t([&] { app.run(port, "127.0.0.1"); });
    wait_for_server(port);
    ServerGuard guard{app, t};

    auto resp = send_and_receive(port,
                                 "POST /sum HTTP/1.1\r\n"
                                 "Host: localhost\r\n"
                                 "Content-Type: application/json\r\n"
                                 "Content-Length: 13\r\n"
                                 "\r\n"
                                 "{invalid json}");
    REQUIRE(extract_status(resp) == 500);
    REQUIRE(extract_body(resp).find("JSON body parse error") != std::string::npos);
}

TEST_CASE("integration 500 internal error from handler", "[integration]") {
    fiasco::server app;
    app.get("/crash", []() -> std::string { throw std::runtime_error("boom"); });

    auto port = find_free_port();
    std::thread t([&] { app.run(port, "127.0.0.1"); });
    wait_for_server(port);
    ServerGuard guard{app, t};

    auto resp = send_and_receive(port, "GET /crash HTTP/1.1\r\nHost: localhost\r\n\r\n");
    REQUIRE(extract_status(resp) == 500);
}

TEST_CASE("integration path parameters", "[integration]") {
    fiasco::server app;
    app.get("/users/{uid}", [](int uid) -> std::string { return "user:" + std::to_string(uid); });

    auto port = find_free_port();
    std::thread t([&] { app.run(port, "127.0.0.1"); });
    wait_for_server(port);
    ServerGuard guard{app, t};

    auto resp = send_and_receive(port, "GET /users/42 HTTP/1.1\r\nHost: localhost\r\n\r\n");
    REQUIRE(extract_status(resp) == 200);
    REQUIRE(extract_body(resp) == "user:42\n");
}

TEST_CASE("integration JSON body deserialization", "[integration]") {
    fiasco::server app;
    app.post("/sum", [](const fiasco::detail::SumInput& input) -> std::string {
        return std::to_string(input.a + input.b);
    });

    auto port = find_free_port();
    std::thread t([&] { app.run(port, "127.0.0.1"); });
    wait_for_server(port);
    ServerGuard guard{app, t};

    auto resp = send_and_receive(port,
                                 "POST /sum HTTP/1.1\r\n"
                                 "Host: localhost\r\n"
                                 "Content-Type: application/json\r\n"
                                 "Content-Length: 15\r\n"
                                 "\r\n"
                                 R"({"a":10,"b":20})");
    REQUIRE(extract_status(resp) == 200);
    REQUIRE(extract_body(resp) == "30\n");
}

TEST_CASE("integration POST empty body with JSON model returns 500", "[integration]") {
    fiasco::server app;
    app.post("/sum", [](const fiasco::detail::SumInput&) -> std::string { return ""; });

    auto port = find_free_port();
    std::thread t([&] { app.run(port, "127.0.0.1"); });
    wait_for_server(port);
    ServerGuard guard{app, t};

    auto resp = send_and_receive(port,
                                 "POST /sum HTTP/1.1\r\n"
                                 "Host: localhost\r\n"
                                 "Content-Type: application/json\r\n"
                                 "Content-Length: 0\r\n"
                                 "\r\n");
    REQUIRE(extract_status(resp) == 500);
    REQUIRE(extract_body(resp).find("empty") != std::string::npos);
}
