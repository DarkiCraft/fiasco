import fiasco.server;
import fiasco.router;

auto root() { return fiasco::json{{"msg", "root"}}; }
auto test() { return fiasco::json{{"msg", "test"}}; }

int main() {
  fiasco::server app;

  app.get("/", root);

  fiasco::router r("/test");
  r.get("/", test);

  app.include_router(r);

  app.run(8080);
}