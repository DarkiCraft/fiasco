# Fiasco

A modern C++17 web framework that tries to bring [FastAPI](https://fastapi.tiangolo.com/) levels of ergonomics into C++ (see usage below)

## Usage

### Compilation

```shell
git clone https://github.com/DarkiCraft/fiasco
cd fiasco
cmake -S . -B build
cmake --build build
ctest --test-dir build

# optional installation
cmake --install build
```

### Basic usage

```c++
fiasco::server app;

app.get("/text", []() {
    return "Hello, World!";
});

app.get("/json", []() {
    return fiasco::json{
        {"message", "Hello, World!"}
    };
});

app.run(8080);
```

### Path parameters

Path parameters are automatically extracted and assigned to function arguments left to right

```c++
app.post("/users/{user_id}/deposit/{amount}", [](int user_id, double amount) {
    // business logic
    // ...
    
    return fiasco::json{
        {"message", "ok"}
    };
});
```
Note that the names of the arguments in the handler do not matter as they are assigned as they appear in the endpoint url

### Returning objects in the response body

Create a struct and use the `FIASCO_MODEL` macro to make it json-serializable, and simply return it.
```c++
struct user_response {
    std::string username;
    int age;
};
FIASCO_MODEL(user_response, username, age)

app.get("/users/{user_id}", [](int user_id) {
    // internal logic
    // ...
    
    return user_response{"John Doe", 21};
});
```

### Passing objects in the request body

Similar to response models, you can create request models in the same way. These are passed in the request body and can be mixed with path parameters.

```c++
struct create_user_request {
    std::string username;
    int age;
};
FIASCO_MODEL(create_user_request, username, age)

using fiasco::response::json;
// first list the path parameters, then the request body model
app.post("/users/{department_id}", [](int department_id, create_user_request req) {
    // ... = req.username;
    // ...
    
    return fiasco::json{
        {"message", "ok"}
    };
});
```

### Dependency injection

Register a dependency once. It's constructed on first use and cached as a singleton for subsequent calls.

```c++
app.provide<db>([]() -> db {
    return db::connect("localhost:5432"); // dummy db
});

// dependencies are passed at the END of the handler arguments as a reference 
app.get("/users/{id}", [](int id, db& database) {
    return database.find(id);
});
```

## Planned Features
- Query parameter parsing
- Middleware pipeline
- Request logging and hooks
- Automatic Docs Generation
- Static file serving
- HTTPS / TLS support
- WebSocket support
- HTTP/2

## Dependencies

- [nlohmann/json](https://github.com/nlohmann/json)
- [llhttp](https://github.com/nodejs/llhttp)
- [Catch2](https://github.com/catchorg/Catch2)


## License

This project is licensed under the [MIT License](LICENSE)