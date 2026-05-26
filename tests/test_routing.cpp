#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include "fiasco/http/request.hpp"
#include "fiasco/http/response.hpp"
#include "fiasco/routing/function_traits.hpp"
#include "fiasco/routing/router.hpp"

// -- Test models --------------------------------------------------------------

struct test_body {
  std::string name;
  int value;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(test_body, name, value)

struct test_response {
  std::string echo;
  int number;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(test_response, echo, number)

// -- Test services (for DI) ---------------------------------------------------

struct counter_service {
  int count = 0;
  int increment() { return ++count; }
};

struct greeter_service {
  std::string prefix;
  std::string greet(const std::string& name) { return prefix + name; }
};

// -- Helpers ------------------------------------------------------------------

static fiasco::request make_request(fiasco::http_method method,
                                    const std::string& path,
                                    const std::string& body = "") {
  fiasco::request req;
  req.method = method;
  req.path = path;
  req.body = body;
  return req;
}

// -- router: static routes ----------------------------------------------------

TEST_CASE("router matches static GET route", "[router]") {
  fiasco::router r;
  r.add_route(fiasco::http_method::get, "/hello",
              [](fiasco::request) { return fiasco::response::to_text("hi"); });

  auto m = r.match(fiasco::http_method::get, "/hello");
  REQUIRE(m.matched);
  REQUIRE(m.path_params.empty());
  REQUIRE(m.ordered_path_params.empty());
}

TEST_CASE("router returns unmatched for unknown path", "[router]") {
  fiasco::router r;
  r.add_route(fiasco::http_method::get, "/hello",
              [](fiasco::request) { return fiasco::response::to_text("hi"); });

  REQUIRE_FALSE(r.match(fiasco::http_method::get, "/goodbye").matched);
}

TEST_CASE("router returns unmatched for wrong method", "[router]") {
  fiasco::router r;
  r.add_route(fiasco::http_method::get, "/hello",
              [](fiasco::request) { return fiasco::response::to_text("hi"); });

  REQUIRE_FALSE(r.match(fiasco::http_method::post, "/hello").matched);
}

TEST_CASE("router::any_method_matches detects 405 case", "[router]") {
  fiasco::router r;
  r.add_route(fiasco::http_method::get, "/hello",
              [](fiasco::request) { return fiasco::response::to_text("hi"); });

  REQUIRE(r.any_method_matches("/hello"));
  REQUIRE_FALSE(r.any_method_matches("/nope"));
}

// -- router: path parameters --------------------------------------------------

TEST_CASE("router extracts single path param", "[router]") {
  fiasco::router r;
  r.add_route(fiasco::http_method::get, "/users/{id}",
              [](fiasco::request) { return fiasco::response::to_text("ok"); });

  auto m = r.match(fiasco::http_method::get, "/users/42");
  REQUIRE(m.matched);
  REQUIRE(m.path_params.at("id") == "42");
  REQUIRE(m.ordered_path_params.size() == 1);
  REQUIRE(m.ordered_path_params[0] == "42");
}

TEST_CASE("router extracts multiple path params in order", "[router]") {
  fiasco::router r;
  r.add_route(fiasco::http_method::get, "/users/{id}/items/{item_id}",
              [](fiasco::request) { return fiasco::response::to_text("ok"); });

  auto m = r.match(fiasco::http_method::get, "/users/7/items/99");
  REQUIRE(m.matched);
  REQUIRE(m.path_params.at("id") == "7");
  REQUIRE(m.path_params.at("item_id") == "99");
  REQUIRE(m.ordered_path_params.size() == 2);
  REQUIRE(m.ordered_path_params[0] == "7");
  REQUIRE(m.ordered_path_params[1] == "99");
}

TEST_CASE("router prefers static route over parameterized", "[router]") {
  fiasco::router r;

  bool static_hit = false;
  r.add_route(fiasco::http_method::get, "/users/me", [&](fiasco::request) {
    static_hit = true;
    return fiasco::response::to_text("me");
  });
  r.add_route(fiasco::http_method::get, "/users/{id}", [](fiasco::request) {
    return fiasco::response::to_text("param");
  });

  auto m = r.match(fiasco::http_method::get, "/users/me");
  REQUIRE(m.matched);
  m.handler(make_request(fiasco::http_method::get, "/users/me"));
  REQUIRE(static_hit);
}

TEST_CASE("router does not match wrong segment count", "[router]") {
  fiasco::router r;
  r.add_route(fiasco::http_method::get, "/users/{id}",
              [](fiasco::request) { return fiasco::response::to_text("ok"); });

  REQUIRE_FALSE(r.match(fiasco::http_method::get, "/users/1/extra").matched);
  REQUIRE_FALSE(r.match(fiasco::http_method::get, "/users").matched);
}

// -- function_traits: raw request passthrough ---------------------------------

TEST_CASE("make_handler passes raw request through", "[function_traits]") {
  fiasco::di_container di;
  auto h = fiasco::make_handler(
      [](const fiasco::request& req) -> fiasco::response {
        return fiasco::response::to_text(req.path);
      },
      di);

  auto res = h(make_request(fiasco::http_method::get, "/hello"));
  REQUIRE(res.status_code == 200);
  REQUIRE(res.body == "/hello");
}

// -- function_traits: path param injection ------------------------------------

TEST_CASE("make_handler injects single int path param", "[function_traits]") {
  fiasco::di_container di;
  auto h = fiasco::make_handler(
      [](int id) -> fiasco::response {
        return fiasco::response::to_text(std::to_string(id));
      },
      di);

  auto req = make_request(fiasco::http_method::get, "/users/42");
  req.ordered_path_params = {"42"};

  REQUIRE(h(req).body == "42");
}

TEST_CASE("make_handler injects multiple positional path params",
          "[function_traits]") {
  fiasco::di_container di;
  auto h = fiasco::make_handler(
      [](int id, double salary) -> fiasco::response {
        return fiasco::response::to_text(std::to_string(id) + "," +
                                         std::to_string(salary));
      },
      di);

  auto req = make_request(fiasco::http_method::get, "/");
  req.ordered_path_params = {"3", "1500"};

  auto res = h(req);
  REQUIRE(res.body.find("3") != std::string::npos);
  REQUIRE(res.body.find("1500") != std::string::npos);
}

TEST_CASE("make_handler injects string path param", "[function_traits]") {
  fiasco::di_container di;
  auto h = fiasco::make_handler(
      [](std::string name) -> fiasco::response {
        return fiasco::response::to_text(name);
      },
      di);

  auto req = make_request(fiasco::http_method::get, "/greet/abdurrahman");
  req.ordered_path_params = {"abdurrahman"};

  REQUIRE(h(req).body == "abdurrahman");
}

// -- function_traits: body deserialization ------------------------------------

TEST_CASE("make_handler deserializes JSON body into model",
          "[function_traits]") {
  fiasco::di_container di;
  auto h = fiasco::make_handler(
      [](test_body b) -> fiasco::response {
        return fiasco::response::to_text(b.name + ":" +
                                         std::to_string(b.value));
      },
      di);

  auto req = make_request(fiasco::http_method::post, "/items",
                          R"({"name":"fiasco","value":42})");

  REQUIRE(h(req).body == "fiasco:42");
}

TEST_CASE("make_handler throws on malformed JSON body", "[function_traits]") {
  fiasco::di_container di;
  auto h = fiasco::make_handler(
      [](test_body b) -> fiasco::response {
        return fiasco::response::to_text("ok");
      },
      di);

  REQUIRE_THROWS(h(make_request(fiasco::http_method::post, "/", "not json")));
}

TEST_CASE("make_handler throws on empty body when model expected",
          "[function_traits]") {
  fiasco::di_container di;
  auto h = fiasco::make_handler(
      [](test_body b) -> fiasco::response {
        return fiasco::response::to_text("ok");
      },
      di);

  REQUIRE_THROWS(h(make_request(fiasco::http_method::post, "/", "")));
}

// -- function_traits: return serialization ------------------------------------

TEST_CASE("make_handler serializes FIASCO_MODEL return to JSON",
          "[function_traits]") {
  fiasco::di_container di;
  auto h = fiasco::make_handler(
      [](const fiasco::request&) -> test_response { return {"hello", 7}; }, di);

  auto res = h(make_request(fiasco::http_method::get, "/"));
  REQUIRE(res.headers.at("Content-Type") == "application/json");

  auto j = nlohmann::json::parse(res.body);
  REQUIRE(j["echo"] == "hello");
  REQUIRE(j["number"] == 7);
}

TEST_CASE("make_handler passes fiasco::response return through unchanged",
          "[function_traits]") {
  fiasco::di_container di;
  auto h = fiasco::make_handler(
      [](const fiasco::request&) -> fiasco::response {
        return fiasco::response::to_text("raw");
      },
      di);

  auto res = h(make_request(fiasco::http_method::get, "/"));
  REQUIRE(res.body == "raw");
  REQUIRE(res.headers.at("Content-Type") == "text/plain");
}

// -- function_traits: mixed signatures ----------------------------------------

TEST_CASE("make_handler handles path param + body together",
          "[function_traits]") {
  fiasco::di_container di;
  auto h = fiasco::make_handler(
      [](int id, test_body b) -> test_response {
        return {b.name, id + b.value};
      },
      di);

  auto req = make_request(fiasco::http_method::post, "/",
                          R"({"name":"fiasco","value":5})");
  req.ordered_path_params = {"10"};

  auto j = nlohmann::json::parse(h(req).body);
  REQUIRE(j["echo"] == "fiasco");
  REQUIRE(j["number"] == 15);
}

// -- DI container: basic resolution -------------------------------------------

TEST_CASE("di_container resolves registered type", "[di]") {
  fiasco::di_container di;
  di.provide<counter_service>([]() { return counter_service{0}; });

  auto& svc = di.resolve<counter_service>();
  REQUIRE(svc.count == 0);
}

TEST_CASE("di_container returns same singleton instance", "[di]") {
  fiasco::di_container di;
  di.provide<counter_service>([]() { return counter_service{0}; });

  auto& a = di.resolve<counter_service>();
  a.increment();
  a.increment();

  auto& b = di.resolve<counter_service>();
  REQUIRE(b.count == 2);
}

TEST_CASE("di_container throws on unregistered type", "[di]") {
  fiasco::di_container di;
  REQUIRE_THROWS(di.resolve<counter_service>());
}

TEST_CASE("di_container::has returns correct availability", "[di]") {
  fiasco::di_container di;
  REQUIRE_FALSE(di.has<counter_service>());
  di.provide<counter_service>([]() { return counter_service{}; });
  REQUIRE(di.has<counter_service>());
}

// -- DI: injection via make_handler -------------------------------------------

TEST_CASE("make_handler injects DI service by reference", "[di]") {
  fiasco::di_container di;
  di.provide<counter_service>([]() { return counter_service{0}; });

  auto h = fiasco::make_handler(
      [](counter_service& svc) -> fiasco::response {
        return fiasco::response::to_text(std::to_string(svc.increment()));
      },
      di);

  auto req = make_request(fiasco::http_method::get, "/");
  REQUIRE(h(req).body == "1");
  REQUIRE(h(req).body == "2");  // same singleton, state persists
  REQUIRE(h(req).body == "3");
}

TEST_CASE("make_handler injects DI service alongside path params", "[di]") {
  fiasco::di_container di;
  di.provide<greeter_service>([]() { return greeter_service{"Hello, "}; });

  auto h = fiasco::make_handler(
      [](std::string name, greeter_service& greeter) -> fiasco::response {
        return fiasco::response::to_text(greeter.greet(name));
      },
      di);

  auto req = make_request(fiasco::http_method::get, "/");
  req.ordered_path_params = {"abdurrahman"};

  REQUIRE(h(req).body == "Hello, abdurrahman");
}

TEST_CASE("make_handler throws on unregistered DI dependency", "[di]") {
  fiasco::di_container di;  // counter_service NOT registered

  auto h = fiasco::make_handler(
      [](counter_service& svc) -> fiasco::response {
        return fiasco::response::to_text("ok");
      },
      di);

  REQUIRE_THROWS(h(make_request(fiasco::http_method::get, "/")));
}

TEST_CASE("make_handler handles path param + body + DI together", "[di]") {
  fiasco::di_container di;
  di.provide<greeter_service>([]() { return greeter_service{"Hi "}; });

  auto h = fiasco::make_handler(
      [](int id, test_body b, greeter_service& greeter) -> test_response {
        return {greeter.greet(b.name), id + b.value};
      },
      di);

  auto req = make_request(fiasco::http_method::post, "/",
                          R"({"name":"fiasco","value":5})");
  req.ordered_path_params = {"10"};

  auto j = nlohmann::json::parse(h(req).body);
  REQUIRE(j["echo"] == "Hi fiasco");
  REQUIRE(j["number"] == 15);
}
