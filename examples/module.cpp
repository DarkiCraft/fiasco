import fiasco.server;

int main() {
    fiasco::server app;
    app.get("/", []() { return fiasco::json{{"msg", "root"}}; });
    app.run(8080);
}