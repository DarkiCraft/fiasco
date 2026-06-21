#include <catch2/catch_test_macros.hpp>

#include <string>

#include "fiasco/internal/router.hpp"

using namespace fiasco::detail;

TEST_CASE("router get route match", "[router]") {
    router r;
    r.get("/hello", [](const request&) -> response { return response::text("world"); });

    auto result = r.match(http_method::get, "/hello");
    REQUIRE(result.matched);

    request req;
    auto res = result.handler(std::move(req));
    REQUIRE(res.status_code == 200);
    REQUIRE(res.body == "world\n");
}

TEST_CASE("router post route", "[router]") {
    router r;
    r.post("/data",
           [](const request&) -> response { return response::json(R"({"status":"ok"})"); });

    auto result = r.match(http_method::post, "/data");
    REQUIRE(result.matched);

    auto res = result.handler(request{});
    REQUIRE(res.status_code == 200);
}

TEST_CASE("router method isolation", "[router]") {
    router r;
    r.get("/item", [](const request&) -> response { return response::empty(); });

    // POST should not match a GET-only route
    auto result = r.match(http_method::post, "/item");
    REQUIRE_FALSE(result.matched);
}

TEST_CASE("router put route", "[router]") {
    router r;
    r.put("/item/{id}", [](const std::string& id) -> std::string { return "updated " + id; });

    auto result = r.match(http_method::put, "/item/99");
    REQUIRE(result.matched);
}

TEST_CASE("router del route", "[router]") {
    router r;
    r.del("/item/{id}", [](int id) -> std::string { return "deleted " + std::to_string(id); });

    auto result = r.match(http_method::del, "/item/42");
    REQUIRE(result.matched);
}

TEST_CASE("router patch route", "[router]") {
    router r;
    r.patch("/item/{id}", [](int id) -> std::string { return "patched " + std::to_string(id); });

    auto result = r.match(http_method::patch, "/item/7");
    REQUIRE(result.matched);
}

TEST_CASE("router include_router with prefix", "[router]") {
    router sub;
    sub.get("/info", [](const request&) -> response { return response::text("sub info"); });

    router main;
    main.include_router(std::move(sub), "/api");

    auto result = main.match(http_method::get, "/api/info");
    REQUIRE(result.matched);

    auto res = result.handler(request{});
    REQUIRE(res.body == "sub info\n");
}

TEST_CASE("router include_router without prefix", "[router]") {
    router sub;
    sub.get("/info", [](const request&) -> response { return response::text("no prefix"); });

    router main;
    main.include_router(std::move(sub));

    auto result = main.match(http_method::get, "/info");
    REQUIRE(result.matched);
}

TEST_CASE("router include_router with sub-router prefix", "[router]") {
    router sub("/v2");
    sub.get("/resource", [](const request&) -> response { return response::text("v2 resource"); });

    router main;
    main.include_router(std::move(sub), "/api");

    auto result = main.match(http_method::get, "/api/v2/resource");
    REQUIRE(result.matched);
}

TEST_CASE("router any_method_matches", "[router]") {
    router r;
    r.get("/only-get", [](const request&) -> response { return response::text(""); });

    REQUIRE(r.any_method_matches("/only-get"));
    REQUIRE_FALSE(r.any_method_matches("/nope"));
}

TEST_CASE("router handler with path params via match", "[router]") {
    router r;
    r.get("/users/{userId}", [](const request& req) -> std::string {
        return "user:" + std::string(req.path_params.at("userId"));
    });

    auto result = r.match(http_method::get, "/users/42");
    REQUIRE(result.matched);
    REQUIRE(result.path_params.size() == 1);
    REQUIRE(result.path_params[0].second == "42");
}

TEST_CASE("router include_router with sub-router own prefix", "[router]") {
    router sub("/v1");
    sub.get("/info", [](const request&) -> response { return response::text("sub info"); });

    router main;
    main.include_router(std::move(sub));

    auto result = main.match(http_method::get, "/v1/info");
    REQUIRE(result.matched);
}

TEST_CASE("router include_router with explicit prefix", "[router]") {
    router sub;
    sub.get("/resource", [](const request&) -> response { return response::text("resource"); });

    router main;
    main.include_router(std::move(sub), "/api");

    auto result = main.match(http_method::get, "/api/resource");
    REQUIRE(result.matched);
}
