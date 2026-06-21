# Fiasco

An expressive C++23 web framework that brings FastAPI-like ergonomics to C++.

> **Status**: Alpha: the core API is functional, but expect breaking changes as features mature.

## Motivation

Existing C++ web frameworks are either syntactically painful (Drogon's verbosity), outdated, poorly documented, or not attractive enough to adopt. Fiasco aims to prove that C++ can be just as enjoyable as Python or Rust for building HTTP services, with the performance you expect from native code.

## Requirements

- **CMake** ≥ 3.28
- **C++23** compiler (GCC 14+, Clang 18+)

## Build

```shell
git clone https://github.com/darkicraft/fiasco
cd fiasco
cmake -S . -B build

# Optional: skip tests
# cmake -S . -B build -DFIASCO_BUILD_TESTS=OFF

# Optional: skip examples
# cmake -S . -B build -DFIASCO_BUILD_EXAMPLES=OFF

# Optional: enable C++20 module support (experimental)
# cmake -S . -B build -DFIASCO_BUILD_MODULES=ON

cmake --build build

# Run the test suite
ctest --test-dir build

# Optional: install
cmake --install build
```

## Features

| Feature | Status |
|---------|--------|
| HTTP/1.1 (GET, POST, PUT, DELETE, PATCH, HEAD, OPTIONS) | ✅ |
| Path parameters with automatic type conversion | ✅ |
| JSON request/response models via `FIASCO_MODEL` | ✅ |
| Type-erased JSON wrapper (no nlohmann headers leaked) | ✅ |
| std::optional, vector, map, tuple serialization | ✅ |
| Sub-router mounting with path prefixes | ✅ |
| Multi-threaded Asio-based async I/O | ✅ |
| Keep-alive connections | ✅ |
| SIGINT/SIGTERM graceful shutdown | ✅ |
| C++20 modules (experimental, opt-in) | ✅ |
| Query parameter parsing | 🚧 Planned |
| Better error handling (custom exception handlers) | 🚧 Planned |
| Automatic API docs generation (OpenAPI / Swagger) | 🚧 Planned |
| Static file serving | 🚧 Planned |
| HTTPS / TLS | 🚧 Planned |
| WebSocket | 🚧 Planned |
| C++26 `std::meta` reflection (replace macros) | 🚧 Future |

## Quick Start

```cpp
#include <fiasco/fiasco.hpp>

int main() {
    fiasco::server app;

    app.get("/hello", []() {
        return "Hello, World!";
    });

    app.get("/json", []() {
        return fiasco::json{
            {"message", "Hello, World!"}
        };
    });
}
```

## Path Parameters

Path parameters are extracted automatically and assigned left-to-right by position.

```cpp
app.post("/users/{user_id}/deposit/{amount}", [](int user_id, double amount) {
    // user_id = 42, amount = 100.50
    return fiasco::json{{"status", "ok"}};
});
```

Supported types: `int`, `long`, `long long`, `float`, `double`, `bool`, `std::string`.

## Request & Response Models

Define a struct, annotate it with `FIASCO_MODEL`, and return it directly.

```cpp
struct UserResponse {
    std::string username;
    int age;
};
FIASCO_MODEL(UserResponse, username, age)

app.get("/users/{id}", [](int id) {
    return UserResponse{"John Doe", 21};
});
```

Request body models work the same way — mixed freely with path parameters.

```cpp
struct CreateUserRequest {
    std::string username;
    int age;
};
FIASCO_MODEL(CreateUserRequest, username, age)

app.post("/users/{department}", [](int department_id, CreateUserRequest req) {
    // department_id from path, req from JSON body
    return fiasco::json{{"created", true}};
});
```

### Supported field types inside FIASCO_MODEL

- Primitive types (`int`, `double`, `bool`, `std::string`, …)
- Nested `FIASCO_MODEL` structs
- `std::vector<T>` and other sequence containers
- `std::map<std::string, T>` and other map containers
- `std::optional<T>` (omitted from JSON → `std::nullopt`)
- `std::pair<T, U>`, `std::tuple<…>`

> **Note**: The `FIASCO_MODEL` macro is a placeholder until C++26 reflection stabilises.
> Once `std::meta` is widely available, model serialisation will be automatic —
> no macros, no boilerplate.

## Response Types

Every handler returns a `fiasco::response`. If you return a plain value (string, int, or a `FIASCO_MODEL` type), it's wrapped automatically. For full control, use the factory methods:

```cpp
app.get("/text", []() {
    return fiasco::response::text("plain text");
});

app.get("/html", []() {
    return fiasco::response::html("<h1>Hello</h1>");
});

app.get("/json", []() {
    return fiasco::response::json(R"({"key":"value"})");
});

app.get("/redirect", []() {
    return fiasco::response::redirect("/new-location");
});

app.get("/error", []() {
    return fiasco::response::error("something went wrong", 400);
});

app.get("/empty", []() {
    return fiasco::response::empty(204);
});
```

Status codes and headers can be set directly on the response struct after construction.

## Sub-routers

Mount groups of routes under a prefix.

```cpp
fiasco::router api("/v1");
api.get("/status", []() { return "ok"; });

fiasco::server app;
app.include_router(std::move(api));
// GET /v1/status now works
```

## Threading

By default, Fiasco spawns `std::thread::hardware_concurrency()` threads (minimum 2). Pass the desired count to the constructor:

```cpp
fiasco::server app(4);  // 4 worker threads
app.run(8080);
```

## Signal Handling

The server catches `SIGINT` and `SIGTERM` internally via Asio's `signal_set`. Pressing Ctrl+C shuts down all worker threads gracefully and returns from `run()`.

## Dependencies

All fetched automatically at configure time:

- [nlohmann/json](https://github.com/nlohmann/json) v3.12 — JSON core
- [llhttp](https://github.com/nodejs/llhttp) v9.2 — HTTP/1.1 parser
- [Asio](https://github.com/chriskohlhoff/asio) 1.38 — async I/O
- [Catch2](https://github.com/catchorg/Catch2) v3.5 (tests only)

## License

MIT — see [LICENSE](LICENSE).
