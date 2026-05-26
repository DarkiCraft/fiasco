#include "fiasco/fiasco.hpp"

int main() {
  fiasco::server app;

  app.get("/", [] { return fiasco::json{{"message", "Hello, World!"}}; });

  app.run(8080);
}
