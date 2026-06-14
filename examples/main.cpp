#include <fiasco/fiasco.hpp>

auto root() {
    return fiasco::json{{"msg", "root"}};
}
auto test() {
    return fiasco::json{{"msg", "test"}};
}
auto test_slash() {
    return fiasco::json{{"msg", "test_slash"}};
}
auto get_user(std::string id) {
    return fiasco::json{{"msg", "user"}, {"id", id}};
}
auto get_user_slash(std::string id) {
    return fiasco::json{{"msg", "user_slash"}, {"id", id}};
}

int main() {
    fiasco::server app;

    app.get("/", root);

    // r1: mounted at its own prefix "/test"
    fiasco::router r1("/test");
    r1.get("/", test);                       // -> /test/
    r1.get("/slash/", test_slash);           // -> /test/slash/  (trailing slash, distinct route)
    r1.get("/users/{id}", get_user);         // no trailing slash
    r1.get("/users/{id}/", get_user_slash);  // trailing slash, distinct route

    // r2: separate instance, mounted under an extra "/api" prefix
    // -> /api/test, /api/test/slash/, /api/test/users/{id}, /api/test/users/{id}/
    fiasco::router r2("/test");
    r2.get("/", test);
    r2.get("/slash/", test_slash);
    r2.get("/users/{id}", get_user);
    r2.get("/users/{id}/", get_user_slash);

    app.include_router(std::move(r1));
    app.include_router(std::move(r2), "/api");

    app.print_routes();

    app.run(8080);
}