#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <cstring>
#include <string>
#include <thread>

#include "fiasco/core/event_loop.hpp"
#include "fiasco/core/tcp_transport.hpp"

/// Helper: get the port a server socket is actually bound to.
static uint16_t get_bound_port(int server_fd) {
  struct sockaddr_in addr {};
  socklen_t len = sizeof(addr);
  ::getsockname(server_fd, reinterpret_cast<struct sockaddr*>(&addr), &len);
  return ntohs(addr.sin_port);
}

/// Helper: connect a blocking client socket to 127.0.0.1:port.
static int connect_to(uint16_t port) {
  int fd = ::socket(AF_INET, SOCK_STREAM, 0);
  struct sockaddr_in addr {};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  ::inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
  ::connect(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr));
  return fd;
}

TEST_CASE("event_loop accepts a connection and reads data", "[event_loop]") {
  fiasco::tcp_transport transport("127.0.0.1", 0);
  uint16_t port = get_bound_port(transport.server_fd());

  fiasco::event_loop loop;
  std::string received;

  // Run the event loop in a background thread
  std::thread server_thread([&] {
    loop.run(transport, [&](int client_fd, fiasco::event_loop& el) {
      char buf[256];
      ssize_t n = ::read(client_fd, buf, sizeof(buf));
      if (n > 0) {
        received.assign(buf, n);
      }
      // Echo back
      if (n > 0) {
        ::write(client_fd, buf, n);
      }
      el.remove_fd(client_fd);
      ::close(client_fd);
      el.stop();
    });
  });

  // Give the server a moment to start
  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  // Connect and send data
  int client = connect_to(port);
  const char* msg = "hello fiasco";
  ::write(client, msg, std::strlen(msg));

  // Read echo
  char echo_buf[256]{};
  ::read(client, echo_buf, sizeof(echo_buf));
  ::close(client);

  server_thread.join();

  REQUIRE(received == "hello fiasco");
  REQUIRE(std::string(echo_buf) == "hello fiasco");
}
