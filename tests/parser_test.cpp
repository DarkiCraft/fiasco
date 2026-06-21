#include <catch2/catch_test_macros.hpp>

#include <string>

#include "fiasco/internal/http/parser.hpp"

using namespace fiasco::detail;

// ============================================================
// Helper: feed a complete HTTP request
// ============================================================

static request parse_raw(const std::string& raw) {
    llhttp_parser parser;
    bool ok = parser.feed(raw.data(), raw.size());
    REQUIRE(ok);
    REQUIRE(parser.is_complete());
    return parser.take_request();
}

// ============================================================
// Basic GET
// ============================================================

TEST_CASE("parser GET root", "[parser]") {
    auto req = parse_raw("GET / HTTP/1.1\r\nHost: localhost\r\n\r\n");
    REQUIRE(req.method == http_method::get);
    REQUIRE(req.path == "/");
    REQUIRE(req.query_string.empty());
    REQUIRE(req.url == "/");
}

TEST_CASE("parser GET path", "[parser]") {
    auto req = parse_raw("GET /users HTTP/1.1\r\nHost: localhost\r\n\r\n");
    REQUIRE(req.method == http_method::get);
    REQUIRE(req.path == "/users");
    REQUIRE(req.url == "/users");
}

// ============================================================
// GET with query string
// ============================================================

TEST_CASE("parser GET with query string", "[parser]") {
    auto req = parse_raw("GET /search?q=hello&page=1 HTTP/1.1\r\nHost: localhost\r\n\r\n");
    REQUIRE(req.method == http_method::get);
    REQUIRE(req.path == "/search");
    REQUIRE(req.query_string == "q=hello&page=1");
    REQUIRE(req.url == "/search?q=hello&page=1");
}

TEST_CASE("parser GET query with no value", "[parser]") {
    auto req = parse_raw("GET /path?flag HTTP/1.1\r\nHost: localhost\r\n\r\n");
    REQUIRE(req.path == "/path");
    REQUIRE(req.query_string == "flag");
}

// ============================================================
// POST with body
// ============================================================

TEST_CASE("parser POST with JSON body", "[parser]") {
    auto req = parse_raw(
        "POST /api/data HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: 24\r\n"
        "\r\n"
        "{\"name\":\"John\",\"age\":30}");
    REQUIRE(req.method == http_method::post);
    REQUIRE(req.path == "/api/data");
    REQUIRE(req.body == "{\"name\":\"John\",\"age\":30}");
    REQUIRE(req.header("Content-Type") == "application/json");
}

// ============================================================
// Headers
// ============================================================

TEST_CASE("parser multiple headers", "[parser]") {
    auto req = parse_raw(
        "GET / HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "Accept: text/html\r\n"
        "User-Agent: test\r\n"
        "\r\n");
    REQUIRE(req.headers.size() >= 3);
    REQUIRE(req.header("Host") == "example.com");
    REQUIRE(req.header("Accept") == "text/html");
    REQUIRE(req.header("User-Agent") == "test");
}

// ============================================================
// Other HTTP methods
// ============================================================

TEST_CASE("parser PUT request", "[parser]") {
    auto req = parse_raw("PUT /update HTTP/1.1\r\nHost: localhost\r\nContent-Length: 0\r\n\r\n");
    REQUIRE(req.method == http_method::put);
}

TEST_CASE("parser DELETE request", "[parser]") {
    auto req = parse_raw("DELETE /resource HTTP/1.1\r\nHost: localhost\r\n\r\n");
    REQUIRE(req.method == http_method::del);
}

TEST_CASE("parser PATCH request", "[parser]") {
    auto req =
        parse_raw("PATCH /resource HTTP/1.1\r\nHost: localhost\r\nContent-Length: 0\r\n\r\n");
    REQUIRE(req.method == http_method::patch);
}

TEST_CASE("parser HEAD request", "[parser]") {
    auto req = parse_raw("HEAD / HTTP/1.1\r\nHost: localhost\r\n\r\n");
    REQUIRE(req.method == http_method::head);
}

// ============================================================
// Errors
// ============================================================

TEST_CASE("parser malformed request", "[parser]") {
    llhttp_parser parser;
    std::string_view bad = "NOT A VALID HTTP REQUEST";
    bool ok = parser.feed(bad.data(), bad.size());
    REQUIRE_FALSE(ok);
    REQUIRE_FALSE(parser.is_complete());
    REQUIRE_FALSE(parser.error().empty());
}

// ============================================================
// Reset and reuse
// ============================================================

TEST_CASE("parser reset and reuse", "[parser]") {
    llhttp_parser parser;

    // First request
    std::string_view req1_raw = "GET /first HTTP/1.1\r\nHost: localhost\r\n\r\n";
    bool ok = parser.feed(req1_raw.data(), req1_raw.size());
    REQUIRE(ok);
    REQUIRE(parser.is_complete());
    auto req1 = parser.take_request();
    REQUIRE(req1.path == "/first");

    // Parser should be reset after take_request()
    REQUIRE_FALSE(parser.is_complete());

    // Second request on same parser
    std::string_view req2_raw =
        "POST /second HTTP/1.1\r\nHost: localhost\r\nContent-Length: 0\r\n\r\n";
    ok = parser.feed(req2_raw.data(), req2_raw.size());
    REQUIRE(ok);
    REQUIRE(parser.is_complete());
    auto req2 = parser.take_request();
    REQUIRE(req2.method == http_method::post);
    REQUIRE(req2.path == "/second");
}

// ============================================================
// Partial feed (multi-chunk)
// ============================================================

TEST_CASE("parser partial feed then complete", "[parser]") {
    llhttp_parser parser;

    std::string_view chunk1 = "GET /path HTTP/1.1\r\nHost: l";
    bool ok = parser.feed(chunk1.data(), chunk1.size());
    REQUIRE(ok);
    REQUIRE_FALSE(parser.is_complete());

    std::string_view chunk2 = "ocalhost\r\n\r\n";
    ok = parser.feed(chunk2.data(), chunk2.size());
    REQUIRE(ok);
    REQUIRE(parser.is_complete());

    auto req = parser.take_request();
    REQUIRE(req.path == "/path");
}

TEST_CASE("parser two requests separate feeds", "[parser]") {
    llhttp_parser parser;

    std::string_view r1 = "GET /first HTTP/1.1\r\nHost: a\r\n\r\n";
    bool ok = parser.feed(r1.data(), r1.size());
    REQUIRE(ok);
    REQUIRE(parser.is_complete());
    auto req1 = parser.take_request();
    REQUIRE(req1.path == "/first");

    std::string_view r2 = "GET /second HTTP/1.1\r\nHost: b\r\n\r\n";
    ok = parser.feed(r2.data(), r2.size());
    REQUIRE(ok);
    REQUIRE(parser.is_complete());
    auto req2 = parser.take_request();
    REQUIRE(req2.path == "/second");
}

TEST_CASE("parser Content-Length incomplete", "[parser]") {
    llhttp_parser parser;

    std::string_view raw =
        "POST /data HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Length: 100\r\n"
        "\r\n"
        "short body";

    bool ok = parser.feed(raw.data(), raw.size());
    REQUIRE(ok);
    // Not complete because only 10 of 100 body bytes arrived
    REQUIRE_FALSE(parser.is_complete());
}

TEST_CASE("parser POST empty body with Content-Length 0", "[parser]") {
    auto req = parse_raw(
        "POST /void HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Length: 0\r\n"
        "\r\n");
    REQUIRE(req.method == http_method::post);
    REQUIRE(req.path == "/void");
    REQUIRE(req.body.empty());
}

TEST_CASE("parser header with empty value", "[parser]") {
    auto req = parse_raw(
        "GET / HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "X-Empty:\r\n"
        "\r\n");
    REQUIRE(req.header("X-Empty").empty());
}

TEST_CASE("parser OPTIONS request", "[parser]") {
    auto req = parse_raw("OPTIONS * HTTP/1.1\r\nHost: localhost\r\n\r\n");
    REQUIRE(req.method == http_method::options);
}
