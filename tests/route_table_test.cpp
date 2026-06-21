#include <catch2/catch_test_macros.hpp>

#include <string>

#include "fiasco/internal/routing/route_table.hpp"

using namespace fiasco::detail;

static handler_fn make_dummy_response(int status, const std::string& body) {
    return [status, body](request) -> response {
        response r;
        r.status_code = status;
        r.body = body;
        return r;
    };
}

// ============================================================
// add and match — static routes
// ============================================================

TEST_CASE("route_table static route match", "[route_table]") {
    route_table table;
    table.add(http_method::get, "/hello", make_dummy_response(200, "world"));

    auto result = table.match(http_method::get, "/hello");
    REQUIRE(result.matched);
    REQUIRE(result.path_params.empty());

    auto res = result.handler(request{});
    REQUIRE(res.status_code == 200);
    REQUIRE(res.body == "world");
}

TEST_CASE("route_table static route no match", "[route_table]") {
    route_table table;
    table.add(http_method::get, "/hello", make_dummy_response(200, ""));

    auto result = table.match(http_method::get, "/nonexistent");
    REQUIRE_FALSE(result.matched);
}

TEST_CASE("route_table static route wrong method", "[route_table]") {
    route_table table;
    table.add(http_method::post, "/item", make_dummy_response(201, ""));

    auto result = table.match(http_method::get, "/item");
    REQUIRE_FALSE(result.matched);
}

// ============================================================
// add and match — parameterized routes
// ============================================================

TEST_CASE("route_table single path param", "[route_table]") {
    route_table table;
    table.add(http_method::get, "/users/{id}", make_dummy_response(200, "found"));

    auto result = table.match(http_method::get, "/users/42");
    REQUIRE(result.matched);
    REQUIRE(result.path_params.size() == 1);
    REQUIRE(result.path_params[0].first == "id");
    REQUIRE(result.path_params[0].second == "42");
}

TEST_CASE("route_table multiple path params", "[route_table]") {
    route_table table;
    table.add(http_method::get, "/users/{uid}/posts/{pid}", make_dummy_response(200, ""));

    auto result = table.match(http_method::get, "/users/99/posts/5");
    REQUIRE(result.matched);
    REQUIRE(result.path_params.size() == 2);
    REQUIRE(result.path_params[0].first == "uid");
    REQUIRE(result.path_params[0].second == "99");
    REQUIRE(result.path_params[1].first == "pid");
    REQUIRE(result.path_params[1].second == "5");
}

TEST_CASE("route_table param route wrong segment count", "[route_table]") {
    route_table table;
    table.add(http_method::get, "/users/{id}", make_dummy_response(200, ""));

    // Wrong number of segments
    auto result = table.match(http_method::get, "/users/42/posts");
    REQUIRE_FALSE(result.matched);

    result = table.match(http_method::get, "/users");
    REQUIRE_FALSE(result.matched);
}

TEST_CASE("route_table param route static segment mismatch", "[route_table]") {
    route_table table;
    table.add(http_method::get, "/users/{id}/profile", make_dummy_response(200, ""));

    auto result = table.match(http_method::get, "/users/42/settings");
    REQUIRE_FALSE(result.matched);
}

// ============================================================
// Mixed static and param routes
// ============================================================

TEST_CASE("route_table static takes priority over param", "[route_table]") {
    route_table table;
    table.add(http_method::get, "/users/all", make_dummy_response(200, "static"));
    table.add(http_method::get, "/users/{id}", make_dummy_response(200, "param"));

    // Static should match first
    auto result = table.match(http_method::get, "/users/all");
    REQUIRE(result.matched);
    REQUIRE(result.path_params.empty());

    // Param should match other paths
    result = table.match(http_method::get, "/users/42");
    REQUIRE(result.matched);
    REQUIRE(result.path_params.size() == 1);
}

// ============================================================
// any_method_matches
// ============================================================

TEST_CASE("route_table any_method_matches with static", "[route_table]") {
    route_table table;
    table.add(http_method::post, "/resource", make_dummy_response(200, ""));

    REQUIRE(table.any_method_matches("/resource"));
    REQUIRE_FALSE(table.any_method_matches("/other"));
}

TEST_CASE("route_table any_method_matches with param", "[route_table]") {
    route_table table;
    table.add(http_method::post, "/items/{id}", make_dummy_response(200, ""));

    REQUIRE(table.any_method_matches("/items/42"));
    REQUIRE_FALSE(table.any_method_matches("/items/42/details"));
}

TEST_CASE("route_table any_method_matches across methods", "[route_table]") {
    route_table table;
    table.add(http_method::get, "/data", make_dummy_response(200, ""));

    // The path exists for GET, so any_method_matches should find it
    REQUIRE(table.any_method_matches("/data"));
}

TEST_CASE("route_table any_method_matches returns false for empty table", "[route_table]") {
    route_table table;
    REQUIRE_FALSE(table.any_method_matches("/anything"));
}

// ============================================================
// for_each_route
// ============================================================

TEST_CASE("route_table for_each_route iterates all routes", "[route_table]") {
    route_table table;
    table.add(http_method::get, "/a", make_dummy_response(200, ""));
    table.add(http_method::post, "/b", make_dummy_response(201, ""));

    int count = 0;
    table.for_each_route([&](http_method, const std::string&) { ++count; });
    REQUIRE(count == 2);
}

TEST_CASE("route_table for_each_route on empty table", "[route_table]") {
    route_table table;
    int count = 0;
    table.for_each_route([&](http_method, const std::string&) { ++count; });
    REQUIRE(count == 0);
}

// ============================================================
// merge_with_prefix
// ============================================================

TEST_CASE("route_table merge with prefix", "[route_table]") {
    route_table sub;
    sub.add(http_method::get, "/resource", make_dummy_response(200, "merged"));

    route_table main;
    main.merge_with_prefix(std::move(sub), "/api/v1");

    auto result = main.match(http_method::get, "/api/v1/resource");
    REQUIRE(result.matched);
}

TEST_CASE("route_table merge with empty prefix", "[route_table]") {
    route_table sub;
    sub.add(http_method::get, "/resource", make_dummy_response(200, ""));

    route_table main;
    main.merge_with_prefix(std::move(sub), "");

    auto result = main.match(http_method::get, "/resource");
    REQUIRE(result.matched);
}

TEST_CASE("route_table merge param routes with prefix", "[route_table]") {
    route_table sub;
    sub.add(http_method::get, "/users/{id}", make_dummy_response(200, ""));

    route_table main;
    main.merge_with_prefix(std::move(sub), "/api");

    auto result = main.match(http_method::get, "/api/users/42");
    REQUIRE(result.matched);
    REQUIRE(result.path_params.size() == 1);
    REQUIRE(result.path_params[0].second == "42");
}

TEST_CASE("route_table only-param route", "[route_table]") {
    route_table table;
    table.add(http_method::get, "/{id}", make_dummy_response(200, ""));

    auto result = table.match(http_method::get, "/42");
    REQUIRE(result.matched);
    REQUIRE(result.path_params.size() == 1);
    REQUIRE(result.path_params[0].second == "42");
}

TEST_CASE("route_table static and param overlap", "[route_table]") {
    route_table table;
    table.add(http_method::get, "/{id}", make_dummy_response(200, "param"));
    table.add(http_method::get, "/static", make_dummy_response(200, "static"));

    auto r1 = table.match(http_method::get, "/static");
    REQUIRE(r1.matched);
    REQUIRE(r1.path_params.empty());

    auto r2 = table.match(http_method::get, "/42");
    REQUIRE(r2.matched);
    REQUIRE(r2.path_params.size() == 1);
}

TEST_CASE("route_table no match on empty table", "[route_table]") {
    route_table table;
    auto result = table.match(http_method::get, "/anything");
    REQUIRE_FALSE(result.matched);
}

TEST_CASE("route_table add multiple methods same path", "[route_table]") {
    route_table table;
    table.add(http_method::get, "/item", make_dummy_response(200, "get"));
    table.add(http_method::post, "/item", make_dummy_response(201, "post"));

    auto r1 = table.match(http_method::get, "/item");
    REQUIRE(r1.matched);

    auto r2 = table.match(http_method::post, "/item");
    REQUIRE(r2.matched);

    auto r3 = table.match(http_method::put, "/item");
    REQUIRE_FALSE(r3.matched);
}
