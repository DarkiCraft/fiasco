#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <map>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

#include "fiasco/internal/types/json.hpp"

using namespace fiasco::detail;
using Catch::Approx;

// ============================================================
// Construction
// ============================================================

TEST_CASE("json_type default construction", "[json]") {
    json_type j;
    REQUIRE(j.is_null());
}

TEST_CASE("json_type nullptr construction", "[json]") {
    json_type j(nullptr);
    REQUIRE(j.is_null());
}

TEST_CASE("json_type bool construction", "[json]") {
    json_type j(true);
    REQUIRE(j.is_boolean());
    REQUIRE(j.get<bool>() == true);

    json_type j2(false);
    REQUIRE(j2.get<bool>() == false);
}

TEST_CASE("json_type integer construction", "[json]") {
    json_type j(42);
    REQUIRE(j.is_number());
    REQUIRE(j.get<int>() == 42);
    REQUIRE(j.get<long>() == 42);
    REQUIRE(j.get<long long>() == 42);
}

TEST_CASE("json_type unsigned construction", "[json]") {
    json_type j(42ul);
    REQUIRE(j.is_number());
    REQUIRE(j.get<unsigned long>() == 42);
}

TEST_CASE("json_type float construction", "[json]") {
    json_type j(3.14f);
    REQUIRE(j.is_number());
    REQUIRE(j.get<float>() == Approx(3.14f));
}

TEST_CASE("json_type double construction", "[json]") {
    json_type j(2.718);
    REQUIRE(j.is_number());
    REQUIRE(j.get<double>() == Approx(2.718));
}

TEST_CASE("json_type string construction", "[json]") {
    json_type j(std::string("hello"));
    REQUIRE(j.is_string());
    REQUIRE(j.get<std::string>() == "hello");
}

TEST_CASE("json_type const char* construction", "[json]") {
    json_type j("world");
    REQUIRE(j.is_string());
    REQUIRE(j.get<std::string>() == "world");
}

TEST_CASE("json_type copy construction", "[json]") {
    json_type original(42);
    json_type copy(original);
    REQUIRE(copy.get<int>() == 42);
    REQUIRE(original.get<int>() == 42);
}

TEST_CASE("json_type move construction", "[json]") {
    json_type original(42);
    json_type moved(std::move(original));
    REQUIRE(moved.get<int>() == 42);
}

// ============================================================
// Assignment
// ============================================================

TEST_CASE("json_type copy assignment", "[json]") {
    json_type a(1);
    json_type b(2);
    a = b;
    REQUIRE(a.get<int>() == 2);
    REQUIRE(b.get<int>() == 2);
}

TEST_CASE("json_type move assignment", "[json]") {
    json_type a(1);
    json_type b(2);
    a = std::move(b);
    REQUIRE(a.get<int>() == 2);
}

// ============================================================
// Object / Array
// ============================================================

TEST_CASE("json_type object factory", "[json]") {
    auto j = json_type::object();
    REQUIRE(j.is_object());
    REQUIRE(j.empty());
    REQUIRE(j.size() == 0);
}

TEST_CASE("json_type array factory", "[json]") {
    auto j = json_type::array({json_type(1), json_type(2), json_type(3)});
    REQUIRE(j.is_array());
    REQUIRE(j.size() == 3);
    REQUIRE(j[0].get<int>() == 1);
    REQUIRE(j[1].get<int>() == 2);
    REQUIRE(j[2].get<int>() == 3);
}

TEST_CASE("json_type initializer_list object", "[json]") {
    json_type j{{"name", json_type("Alice")}, {"age", json_type(30)}};
    REQUIRE(j.is_object());
    REQUIRE(j.size() == 2);
    REQUIRE(j["name"].get<std::string>() == "Alice");
    REQUIRE(j["age"].get<int>() == 30);
}

TEST_CASE("json_type empty array factory", "[json]") {
    auto j = json_type::array({});
    REQUIRE(j.is_array());
    REQUIRE(j.empty());
}

// ============================================================
// Element access (shared mutable path)
// ============================================================

TEST_CASE("json_type mutable operator[] string key", "[json]") {
    json_type j = json_type::object();
    j["key"] = json_type("value");
    REQUIRE(j["key"].get<std::string>() == "value");
}

TEST_CASE("json_type mutable operator[] index", "[json]") {
    json_type j = json_type::array({json_type(10), json_type(20)});
    REQUIRE(j[0].get<int>() == 10);
    REQUIRE(j[1].get<int>() == 20);
}

TEST_CASE("json_type mutable operator[] nested access", "[json]") {
    json_type j = json_type::object();
    j["outer"] = json_type::object();
    j["outer"]["inner"] = json_type(99);
    REQUIRE(j["outer"]["inner"].get<int>() == 99);
}

TEST_CASE("json_type const operator[] detached copy", "[json]") {
    json_type j = json_type::object();
    j["key"] = json_type(42);

    const json_type& cj = j;
    auto val = cj["key"];
    REQUIRE(val.get<int>() == 42);

    // Verify the const accessor doesn't affect the original's path
    REQUIRE(j["key"].get<int>() == 42);
}

// ============================================================
// Type introspection
// ============================================================

TEST_CASE("json_type is_null", "[json]") {
    json_type j;
    REQUIRE(j.is_null());
    REQUIRE_FALSE(j.is_boolean());
    REQUIRE_FALSE(j.is_number());
    REQUIRE_FALSE(j.is_string());
    REQUIRE_FALSE(j.is_object());
    REQUIRE_FALSE(j.is_array());
}

TEST_CASE("json_type type checks", "[json]") {
    json_type b(true);
    REQUIRE(b.is_boolean());

    json_type n(42);
    REQUIRE(n.is_number());

    json_type s("text");
    REQUIRE(s.is_string());

    json_type o = json_type::object();
    REQUIRE(o.is_object());

    json_type a = json_type::array({});
    REQUIRE(a.is_array());
}

// ============================================================
// Query
// ============================================================

TEST_CASE("json_type contains", "[json]") {
    json_type j{{"x", json_type(1)}};
    REQUIRE(j.contains("x"));
    REQUIRE_FALSE(j.contains("y"));
}

TEST_CASE("json_type empty and size", "[json]") {
    json_type obj = json_type::object();
    REQUIRE(obj.empty());
    REQUIRE(obj.size() == 0);

    obj["a"] = json_type(1);
    REQUIRE_FALSE(obj.empty());
    REQUIRE(obj.size() == 1);

    json_type arr = json_type::array({json_type(1), json_type(2)});
    REQUIRE_FALSE(arr.empty());
    REQUIRE(arr.size() == 2);
}

// ============================================================
// value() with default
// ============================================================

TEST_CASE("json_type value with default", "[json]") {
    json_type j{{"key", json_type(42)}};
    REQUIRE(j.value("key", 0) == 42);
    REQUIRE(j.value("nonexistent", 99) == 99);
}

// ============================================================
// Serialization
// ============================================================

TEST_CASE("json_type parse and dump", "[json]") {
    auto j = json_type::parse(R"({"name":"Alice","age":30})");
    REQUIRE(j.is_object());
    REQUIRE(j["name"].get<std::string>() == "Alice");
    REQUIRE(j["age"].get<int>() == 30);
}

TEST_CASE("json_type parse invalid JSON throws", "[json]") {
    REQUIRE_THROWS_AS(json_type::parse("{invalid}"), json_type::exception);
}

TEST_CASE("json_type dump compact", "[json]") {
    json_type j{{"a", json_type(1)}};
    auto s = j.dump();
    REQUIRE(s == R"({"a":1})");
}

TEST_CASE("json_type dump pretty", "[json]") {
    json_type j{{"a", json_type(1)}};
    auto s = j.dump(4);
    REQUIRE(s.find("\"a\"") != std::string::npos);
    REQUIRE(s.find("1") != std::string::npos);
}

// ============================================================
// Mutation
// ============================================================

TEST_CASE("json_type push_back", "[json]") {
    auto j = json_type::array({});
    j.push_back(json_type(1));
    j.push_back(json_type(2));
    REQUIRE(j.size() == 2);
    REQUIRE(j[0].get<int>() == 1);
    REQUIRE(j[1].get<int>() == 2);
}

TEST_CASE("json_type erase", "[json]") {
    json_type j{{"a", json_type(1)}, {"b", json_type(2)}};
    REQUIRE(j.size() == 2);
    j.erase("a");
    REQUIRE(j.size() == 1);
    REQUIRE_FALSE(j.contains("a"));
    REQUIRE(j.contains("b"));
}

TEST_CASE("json_type clear", "[json]") {
    json_type j{{"a", json_type(1)}};
    REQUIRE_FALSE(j.empty());
    j.clear();
    REQUIRE(j.empty());
}

TEST_CASE("json_type merge_patch", "[json]") {
    json_type original{{"a", json_type(1)}, {"b", json_type(2)}};
    json_type patch{{"b", json_type(99)}, {"c", json_type(3)}};
    original.merge_patch(patch);

    REQUIRE(original["a"].get<int>() == 1);
    REQUIRE(original["b"].get<int>() == 99);
    REQUIRE(original["c"].get<int>() == 3);
}

// ============================================================
// Object keys
// ============================================================

TEST_CASE("json_type object_keys", "[json]") {
    json_type j{{"z", json_type(1)}, {"a", json_type(2)}};
    auto keys = j.object_keys();
    REQUIRE(keys.size() == 2);
    // Should contain both keys
    REQUIRE(std::find(keys.begin(), keys.end(), "z") != keys.end());
    REQUIRE(std::find(keys.begin(), keys.end(), "a") != keys.end());
}

TEST_CASE("json_type object_keys on non-object returns empty", "[json]") {
    json_type j(42);
    auto keys = j.object_keys();
    REQUIRE(keys.empty());
}

// ============================================================
// Comparison
// ============================================================

TEST_CASE("json_type equality", "[json]") {
    json_type a(42);
    json_type b(42);
    json_type c(99);
    REQUIRE(a == b);
    REQUIRE_FALSE(a == c);
    REQUIRE(a != c);
}

TEST_CASE("json_type equality objects", "[json]") {
    json_type a{{"x", json_type(1)}};
    json_type b{{"x", json_type(1)}};
    REQUIRE(a == b);
}

TEST_CASE("json_type inequality different types", "[json]") {
    json_type n(42);
    json_type s("42");
    REQUIRE(n != s);
}

// ============================================================
// Primitive ADL from_json / to_json
// ============================================================

TEST_CASE("to_json primitive types", "[json]") {
    json_type j;
    to_json(j, 42);
    REQUIRE(j.get<int>() == 42);

    to_json(j, 3.14);
    REQUIRE(j.get<double>() == Approx(3.14));

    to_json(j, true);
    REQUIRE(j.get<bool>() == true);

    to_json(j, std::string("test"));
    REQUIRE(j.get<std::string>() == "test");

    to_json(j, "cstr");
    REQUIRE(j.get<std::string>() == "cstr");
}

TEST_CASE("from_json primitive types", "[json]") {
    int iv = 0;
    from_json(json_type(42), iv);
    REQUIRE(iv == 42);

    double dv = 0;
    from_json(json_type(3.14), dv);
    REQUIRE(dv == Approx(3.14));

    bool bv = false;
    from_json(json_type(true), bv);
    REQUIRE(bv == true);

    std::string sv;
    from_json(json_type("hello"), sv);
    REQUIRE(sv == "hello");
}

// ============================================================
// Template constructor (ADL to_json)
// ============================================================

TEST_CASE("json_type template constructor with primitives", "[json]") {
    json_type j(42);
    REQUIRE(j.is_number());

    json_type j2(std::string("hello"));
    REQUIRE(j2.is_string());
}

// ============================================================
// Sequence<T> (vectors, etc.)
// ============================================================

TEST_CASE("to_json with vector<int>", "[json]") {
    std::vector<int> v = {1, 2, 3};
    json_type j;
    to_json(j, v);
    REQUIRE(j.is_array());
    REQUIRE(j.size() == 3);
    REQUIRE(j[0].get<int>() == 1);
    REQUIRE(j[1].get<int>() == 2);
    REQUIRE(j[2].get<int>() == 3);
}

TEST_CASE("from_json to vector<int>", "[json]") {
    json_type j = json_type::array({json_type(10), json_type(20), json_type(30)});
    std::vector<int> v;
    from_json(j, v);
    REQUIRE(v.size() == 3);
    REQUIRE(v[0] == 10);
    REQUIRE(v[1] == 20);
    REQUIRE(v[2] == 30);
}

TEST_CASE("to_json with vector<string>", "[json]") {
    std::vector<std::string> v = {"a", "b"};
    json_type j;
    to_json(j, v);
    REQUIRE(j.is_array());
    REQUIRE(j[0].get<std::string>() == "a");
    REQUIRE(j[1].get<std::string>() == "b");
}

// ============================================================
// Map<T> (unordered_map)
// ============================================================

TEST_CASE("to_json with map", "[json]") {
    std::map<std::string, int> m = {{"x", 1}, {"y", 2}};
    json_type j;
    to_json(j, m);
    REQUIRE(j.is_object());
    REQUIRE(j["x"].get<int>() == 1);
    REQUIRE(j["y"].get<int>() == 2);
}

TEST_CASE("from_json to map", "[json]") {
    json_type j{{"a", json_type(10)}, {"b", json_type(20)}};
    std::map<std::string, int> m;
    from_json(j, m);
    REQUIRE(m.size() == 2);
    REQUIRE(m["a"] == 10);
    REQUIRE(m["b"] == 20);
}

// ============================================================
// Optional<T>
// ============================================================

TEST_CASE("to_json with optional (has_value)", "[json]") {
    std::optional<int> opt(42);
    json_type j;
    to_json(j, opt);
    REQUIRE(j.get<int>() == 42);
}

TEST_CASE("to_json with optional (nullopt)", "[json]") {
    std::optional<int> opt;
    json_type j;
    to_json(j, opt);
    REQUIRE(j.is_null());
}

TEST_CASE("from_json to optional (value)", "[json]") {
    std::optional<int> opt;
    from_json(json_type(99), opt);
    REQUIRE(opt.has_value());
    REQUIRE(opt.value() == 99);
}

TEST_CASE("from_json to optional (null)", "[json]") {
    std::optional<int> opt;
    from_json(json_type(nullptr), opt);
    REQUIRE_FALSE(opt.has_value());
}

// ============================================================
// TupleLike<T> (pair, tuple)
// ============================================================

TEST_CASE("to_json with pair", "[json]") {
    auto p = std::make_pair(1, "hello");
    json_type j;
    to_json(j, p);
    REQUIRE(j.is_array());
    REQUIRE(j.size() == 2);
    REQUIRE(j[0].get<int>() == 1);
    REQUIRE(j[1].get<std::string>() == "hello");
}

TEST_CASE("from_json to pair", "[json]") {
    json_type j = json_type::array({json_type(42), json_type("test")});
    std::pair<int, std::string> p;
    from_json(j, p);
    REQUIRE(p.first == 42);
    REQUIRE(p.second == "test");
}

TEST_CASE("to_json with tuple", "[json]") {
    auto t = std::make_tuple(1, 2.5, "three");
    json_type j;
    to_json(j, t);
    REQUIRE(j.is_array());
    REQUIRE(j.size() == 3);
    REQUIRE(j[0].get<int>() == 1);
    REQUIRE(j[1].get<double>() == Approx(2.5));
    REQUIRE(j[2].get<std::string>() == "three");
}

TEST_CASE("from_json to tuple", "[json]") {
    json_type j = json_type::array({json_type(10), json_type(3.14), json_type("x")});
    std::tuple<int, double, std::string> t;
    from_json(j, t);
    REQUIRE(std::get<0>(t) == 10);
    REQUIRE(std::get<1>(t) == Approx(3.14));
    REQUIRE(std::get<2>(t) == "x");
}

// ============================================================
// data() bridge
// ============================================================

TEST_CASE("json_type data() bridge returns non-null", "[json]") {
    json_type j(42);
    REQUIRE(j.data() != nullptr);
    const json_type& cj = j;
    REQUIRE(cj.data() != nullptr);
}

TEST_CASE("json_type mutable path mutation modifies root", "[json]") {
    json_type root = json_type::object();
    root["nested"] = json_type::object();

    // Get a shared view via mutable operator[]
    auto view = root["nested"];
    view["key"] = json_type(99);

    // The root should reflect the mutation through the shared path
    REQUIRE(root["nested"]["key"].get<int>() == 99);
}

TEST_CASE("json_type const operator[] creates detached copy", "[json]") {
    json_type root = json_type::object();
    root["key"] = json_type(42);

    // Get a detached copy via const operator[]
    const json_type& const_root = root;
    auto copy = const_root["key"];

    // Modify the root
    root["key"] = json_type(0);

    // The copy should still have the original value
    REQUIRE(copy.get<int>() == 42);
}

TEST_CASE("json_type mutable array index modifies root", "[json]") {
    json_type arr = json_type::array({json_type(1), json_type(2), json_type(3)});

    auto elem = arr[1];
    elem = json_type(99);

    REQUIRE(arr[1].get<int>() == 99);
}

TEST_CASE("json_type nested array and object path", "[json]") {
    json_type root = json_type::object();
    root["items"] = json_type::array({json_type::object()});
    root["items"][0]["id"] = json_type(7);

    REQUIRE(root["items"][0]["id"].get<int>() == 7);
}
