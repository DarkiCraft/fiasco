#include <fiasco/fiasco.hpp>

int main() {
    fiasco::server app;

    app.get("/", []() { return "Hello, World!"; });

    app.run(8080);
}