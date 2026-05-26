#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include "fiasco/http/parser.hpp"
#include "fiasco/http/request.hpp"
#include "fiasco/http/response.hpp"

// -- request tests -----------------------------------------------------------

TEST_CASE("method_from_string converts correctly", "[http]") {
  REQUIRE(fiasco::string_to_method("GET") == fiasco::http_method::get);
  REQUIRE(fiasco::string_to_method("POST") == fiasco::http_method::post);
  REQUIRE(fiasco::string_to_method("DELETE") == fiasco::http_method::del);
  REQUIRE(fiasco::string_to_method("GARBAGE") == fiasco::http_method::unknown);
}

TEST_CASE("method_to_string round-trips", "[http]") {
  REQUIRE(std::string(fiasco::method_to_string(fiasco::http_method::get)) ==
          "GET");
  REQUIRE(std::string(fiasco::method_to_string(fiasco::http_method::del)) ==
          "DELETE");
}

TEST_CASE("request::header returns value or empty", "[http]") {
  fiasco::request req;
  req.headers["Content-Type"] = "application/json";

  REQUIRE(req.header("Content-Type") == "application/json");
  REQUIRE(req.content_type() == "application/json");
  REQUIRE(req.header("X-Missing") == "");
}

// -- response tests ----------------------------------------------------------

TEST_CASE("response::text creates plain text response", "[http]") {
  auto r = fiasco::response::to_text("hello");
  REQUIRE(r.status_code == 200);
  REQUIRE(r.body == "hello");
  REQUIRE(r.headers["Content-Type"] == "text/plain");
}

TEST_CASE("response::json creates JSON response", "[http]") {
  auto r = fiasco::response::to_json(R"({"ok":true})", 201);
  REQUIRE(r.status_code == 201);
  REQUIRE(r.body == "{\"ok\":true}");
  REQUIRE(r.headers["Content-Type"] == "application/json");
}

TEST_CASE("response::error creates error response", "[http]") {
  auto r = fiasco::response::to_error("not found", 404);
  REQUIRE(r.status_code == 404);
  REQUIRE(r.headers["Content-Type"] == "application/json");

  // Body must be valid JSON with an "error" key.
  auto j = nlohmann::json::parse(r.body);  // throws on invalid JSON
  REQUIRE(j["error"] == "not found");
}

TEST_CASE("response::error escapes special characters in message", "[http]") {
  // The old manual concatenation would have produced broken JSON here.
  const std::string tricky = R"(path "C:\foo\bar" not found)";
  auto r = fiasco::response::to_error(tricky, 500);

  // Must parse as valid JSON — no throw.
  auto j = nlohmann::json::parse(r.body);

  // Round-trip must preserve the original string exactly.
  REQUIRE(j["error"] == tricky);
}

TEST_CASE("response::serialize produces valid HTTP/1.1", "[http]") {
  auto r = fiasco::response::to_text("hi");
  auto raw = r.serialize();

  REQUIRE(raw.find("HTTP/1.1 200 OK\r\n") == 0);
  REQUIRE(raw.find("Content-Type: text/plain\r\n") != std::string::npos);
  REQUIRE(raw.find("Content-Length: 2\r\n") != std::string::npos);
  REQUIRE(raw.find("Connection: close\r\n") != std::string::npos);
  REQUIRE(raw.find("\r\n\r\nhi") != std::string::npos);
}

// -- parser tests ------------------------------------------------------------

TEST_CASE("llhttp_parser parses a simple GET request", "[parser]") {
  fiasco::llhttp_parser parser;

  const char* raw =
      "GET /users?sort=name HTTP/1.1\r\n"
      "Host: localhost\r\n"
      "Accept: text/html\r\n"
      "\r\n";

  REQUIRE(parser.feed(raw, std::strlen(raw)));
  REQUIRE(parser.is_complete());

  auto req = parser.take_request();
  REQUIRE(req.method == fiasco::http_method::get);
  REQUIRE(req.path == "/users");
  REQUIRE(req.query_string == "sort=name");
  REQUIRE(req.url == "/users?sort=name");
  REQUIRE(req.header("Host") == "localhost");
  REQUIRE(req.header("Accept") == "text/html");
  REQUIRE(req.body.empty());
}

TEST_CASE("llhttp_parser parses a POST request with body", "[parser]") {
  fiasco::llhttp_parser parser;

  std::string body = "{\"name\":\"fiasco\",\"version\":1}";
  std::string raw =
      "POST /data HTTP/1.1\r\n"
      "Host: localhost\r\n"
      "Content-Type: application/json\r\n"
      "Content-Length: " +
      std::to_string(body.size()) +
      "\r\n"
      "\r\n" +
      body;

  REQUIRE(parser.feed(raw.c_str(), raw.size()));
  REQUIRE(parser.is_complete());

  auto req = parser.take_request();
  REQUIRE(req.method == fiasco::http_method::post);
  REQUIRE(req.path == "/data");
  REQUIRE(req.content_type() == "application/json");
  REQUIRE(req.body == body);
}

TEST_CASE("llhttp_parser resets for reuse", "[parser]") {
  fiasco::llhttp_parser parser;

  const char* raw1 = "GET / HTTP/1.1\r\nHost: a\r\n\r\n";
  REQUIRE(parser.feed(raw1, std::strlen(raw1)));
  REQUIRE(parser.is_complete());
  auto req1 = parser.take_request();

  const char* raw2 =
      "POST /x HTTP/1.1\r\nHost: b\r\nContent-Length: 2\r\n\r\nhi";
  REQUIRE(parser.feed(raw2, std::strlen(raw2)));
  REQUIRE(parser.is_complete());
  auto req2 = parser.take_request();

  REQUIRE(req1.method == fiasco::http_method::get);
  REQUIRE(req1.path == "/");
  REQUIRE(req2.method == fiasco::http_method::post);
  REQUIRE(req2.path == "/x");
  REQUIRE(req2.body == "hi");
}

TEST_CASE("llhttp_parser reports errors on malformed input", "[parser]") {
  fiasco::llhttp_parser parser;
  const char* garbage = "NOT_HTTP garbage\r\n\r\n";
  REQUIRE_FALSE(parser.feed(garbage, std::strlen(garbage)));
  REQUIRE_FALSE(parser.error().empty());
}
