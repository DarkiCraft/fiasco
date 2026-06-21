#include <fiasco/fiasco.hpp>

int main() {
    fiasco::server app(6);

    app.get("/", []() { return "Hello, World!"; });

    app.run(8080);
}