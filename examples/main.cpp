#include "fiasco/fiasco.hpp"

int main() {
  fiasco::server app;

  // noise routes
  for (int i = 0; i < 100; i++) {
    app.get("/noise/" + std::to_string(i), [] { return 42; });
  }

  app.get("/", [] { return fiasco::json{{"message", "Hello, World!"}}; });

  app.run(8080);
}
