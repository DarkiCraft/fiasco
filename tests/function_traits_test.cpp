#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <string>

#include "fiasco/internal/routing/function_traits.hpp"

using namespace fiasco::detail;

// ============================================================
// convert_path_param
// ============================================================

TEST_CASE("convert_path_param string", "[function_traits]") {
    auto s = convert_path_param<std::string>("hello");
    REQUIRE(s == "hello");
}

TEST_CASE("convert_path_param int", "[function_traits]") {
    auto v = convert_path_param<int>("42");
    REQUIRE(v == 42);
}

TEST_CASE("convert_path_param long", "[function_traits]") {
    auto v = convert_path_param<long>("2147483648");
    REQUIRE(v == 2147483648L);
}

TEST_CASE("convert_path_param float", "[function_traits]") {
    auto v = convert_path_param<float>("3.14");
    REQUIRE(v == Catch::Approx(3.14f));
}

TEST_CASE("convert_path_param double", "[function_traits]") {
    auto v = convert_path_param<double>("2.718");
    REQUIRE(v == Catch::Approx(2.718));
}

TEST_CASE("convert_path_param bool true", "[function_traits]") {
    REQUIRE(convert_path_param<bool>("true") == true);
    REQUIRE(convert_path_param<bool>("1") == true);
}

TEST_CASE("convert_path_param bool false", "[function_traits]") {
    REQUIRE(convert_path_param<bool>("false") == false);
    REQUIRE(convert_path_param<bool>("0") == false);
    REQUIRE(convert_path_param<bool>("anything") == false);
}

TEST_CASE("convert_path_param int throws on bad input", "[function_traits]") {
    REQUIRE_THROWS_AS(convert_path_param<int>("not_a_number"), std::runtime_error);
    REQUIRE_THROWS_AS(convert_path_param<int>(""), std::runtime_error);
}

// ============================================================
// serialize_return
// ============================================================

TEST_CASE("serialize_return returns response as-is", "[function_traits]") {
    auto res = response::text("hello");
    auto result = serialize_return(res);
    REQUIRE(result.status_code == 200);
    REQUIRE(result.body == "hello\n");
}

TEST_CASE("serialize_return string to text response", "[function_traits]") {
    auto result = serialize_return(std::string("test"));
    REQUIRE(result.status_code == 200);
    REQUIRE(result.headers.at("Content-Type") == "text/plain");
}

TEST_CASE("serialize_return const char*", "[function_traits]") {
    auto result = serialize_return("cstr");
    REQUIRE(result.status_code == 200);
}

TEST_CASE("serialize_return int", "[function_traits]") {
    auto result = serialize_return(42);
    REQUIRE(result.status_code == 200);
    REQUIRE(result.body.find("42") != std::string::npos);
}

TEST_CASE("serialize_return double", "[function_traits]") {
    auto result = serialize_return(3.14);
    REQUIRE(result.status_code == 200);
}

// ============================================================
// Static checks for lambda_traits
// ============================================================

TEST_CASE("lambda_traits return type", "[function_traits]") {
    auto lambda = []() -> std::string { return "hi"; };
    using traits = lambda_traits<decltype(lambda)>;
    bool same = std::is_same_v<traits::return_type, std::string>;
    REQUIRE(same);
}

TEST_CASE("lambda_traits arity", "[function_traits]") {
    auto f0 = []() { return 1; };
    REQUIRE(lambda_traits<decltype(f0)>::arity == 0);

    auto f1 = [](int) { return 1; };
    REQUIRE(lambda_traits<decltype(f1)>::arity == 1);

    auto f2 = [](int, std::string) { return 1; };
    REQUIRE(lambda_traits<decltype(f2)>::arity == 2);
}

// ============================================================
// make_handler — wraps callables into handler_fn
// ============================================================

TEST_CASE("make_handler no-arg returns string", "[function_traits]") {
    auto handler = make_handler([]() -> std::string { return "hello from handler"; });

    request req;
    auto res = handler(std::move(req));
    REQUIRE(res.status_code == 200);
    REQUIRE(res.body == "hello from handler\n");
}

TEST_CASE("make_handler no-arg returns void", "[function_traits]") {
    bool called = false;
    auto handler = make_handler([&called]() { called = true; });

    request req;
    auto res = handler(std::move(req));
    REQUIRE(called);
    REQUIRE(res.status_code == 204);  // response::empty()
}

TEST_CASE("make_handler captures request", "[function_traits]") {
    auto handler =
        make_handler([](const request& req) -> std::string { return req.header("X-Test"); });

    request req;
    req.headers["X-Test"] = "captured";
    auto res = handler(std::move(req));
    REQUIRE(res.body == "captured\n");
}

TEST_CASE("make_handler with path params via ordered_path_params", "[function_traits]") {
    auto handler = make_handler([](int id, std::string name) -> std::string {
        return "id=" + std::to_string(id) + " name=" + name;
    });

    request req;
    req.ordered_path_params.push_back("42");
    req.ordered_path_params.push_back("alice");
    auto res = handler(std::move(req));
    REQUIRE(res.body.find("id=42") != std::string::npos);
    REQUIRE(res.body.find("name=alice") != std::string::npos);
}

TEST_CASE("make_handler throws on insufficient path params", "[function_traits]") {
    auto handler = make_handler([](int id) -> std::string { return std::to_string(id); });

    request req;
    // No path params set
    REQUIRE_THROWS_AS(handler(std::move(req)), std::runtime_error);
}

TEST_CASE("make_handler with body deserialization", "[function_traits]") {
    auto handler =
        make_handler([](int value) -> std::string { return "got=" + std::to_string(value); });

    request req;
    req.body = R"({"value": 99})";
    req.ordered_path_params.push_back("42");

    // The handler has (int) — since int is Primitive, it consumes the path param,
    // not the body (FromJson path)
    // It will use ordered_path_params[0] = "42"
    auto res = handler(std::move(req));
    REQUIRE(res.body.find("got=42") != std::string::npos);
}

// ============================================================
// make_handler with function pointers
// ============================================================

static std::string free_function(int x) {
    return "free:" + std::to_string(x);
}

TEST_CASE("make_handler with function pointer", "[function_traits]") {
    auto handler = make_handler(free_function);

    request req;
    req.ordered_path_params.push_back("77");
    auto res = handler(std::move(req));
    REQUIRE(res.body.find("free:77") != std::string::npos);
}

// ============================================================
// make_handler — error propagation
// ============================================================

TEST_CASE("make_handler handler exception propagates", "[function_traits]") {
    auto handler = make_handler([]() -> std::string { throw std::runtime_error("handler error"); });

    request req;
    REQUIRE_THROWS_AS(handler(std::move(req)), std::runtime_error);
}
