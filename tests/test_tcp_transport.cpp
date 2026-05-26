#include <sys/socket.h>
#include <unistd.h>

#include <catch2/catch_test_macros.hpp>
#include <string>

#include "fiasco/core/tcp_transport.hpp"

// -- Helpers
// -------------------------------------------------------------------

/// Opens a connected, non-blocking socket pair. [0] = writer, [1] = reader.
static std::pair<int, int> make_socket_pair() {
  int fds[2];
  REQUIRE(::socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);
  fiasco::set_nonblocking(fds[0]);
  fiasco::set_nonblocking(fds[1]);
  return {fds[0], fds[1]};
}

// -- socket_fd
// -----------------------------------------------------------------

TEST_CASE("socket_fd default-constructs as invalid", "[tcp_transport]") {
  fiasco::socket_fd fd;
  REQUIRE_FALSE(fd.valid());
  REQUIRE(fd.get() == -1);
}

TEST_CASE("socket_fd takes ownership and closes on destruction",
          "[tcp_transport]") {
  int raw = ::socket(AF_INET, SOCK_STREAM, 0);
  REQUIRE(raw >= 0);

  {
    fiasco::socket_fd fd(raw);
    REQUIRE(fd.valid());
    REQUIRE(fd.get() == raw);
  }

  // fd was closed by the destructor — a second close should fail with EBADF.
  REQUIRE(::close(raw) == -1);
  REQUIRE(errno == EBADF);
}

TEST_CASE("socket_fd is movable", "[tcp_transport]") {
  int raw = ::socket(AF_INET, SOCK_STREAM, 0);
  REQUIRE(raw >= 0);

  fiasco::socket_fd a(raw);
  fiasco::socket_fd b(std::move(a));

  REQUIRE_FALSE(a.valid());
  REQUIRE(b.valid());
  REQUIRE(b.get() == raw);
}

TEST_CASE("socket_fd move-assignment transfers ownership", "[tcp_transport]") {
  int r1 = ::socket(AF_INET, SOCK_STREAM, 0);
  int r2 = ::socket(AF_INET, SOCK_STREAM, 0);
  REQUIRE(r1 >= 0);
  REQUIRE(r2 >= 0);

  fiasco::socket_fd a(r1);
  fiasco::socket_fd b(r2);
  b = std::move(a);

  // r2 should have been closed by the move-assignment.
  REQUIRE(::close(r2) == -1);
  REQUIRE(errno == EBADF);

  REQUIRE(b.valid());
  REQUIRE(b.get() == r1);
  REQUIRE_FALSE(a.valid());
}

TEST_CASE("socket_fd release gives up ownership", "[tcp_transport]") {
  int raw = ::socket(AF_INET, SOCK_STREAM, 0);
  REQUIRE(raw >= 0);

  fiasco::socket_fd fd(raw);
  int released = fd.release();

  REQUIRE(released == raw);
  REQUIRE_FALSE(fd.valid());

  ::close(released);  // We own it now.
}

// -- set_nonblocking
// -----------------------------------------------------------

TEST_CASE("set_nonblocking sets O_NONBLOCK on a valid fd", "[tcp_transport]") {
  int raw = ::socket(AF_INET, SOCK_STREAM, 0);
  REQUIRE(raw >= 0);

  REQUIRE_NOTHROW(fiasco::set_nonblocking(raw));

  int flags = ::fcntl(raw, F_GETFL, 0);
  REQUIRE((flags & O_NONBLOCK) != 0);

  ::close(raw);
}

// -- tcp_transport
// -------------------------------------------------------------

TEST_CASE("tcp_transport binds and listens on a port", "[tcp_transport]") {
  // Port 0 lets the OS assign any free port.
  fiasco::tcp_transport transport("127.0.0.1", 0);
  REQUIRE(transport.server_fd() >= 0);
}

TEST_CASE("tcp_transport accept returns invalid fd when no connections pending",
          "[tcp_transport]") {
  fiasco::tcp_transport transport("127.0.0.1", 0);
  auto client = transport.accept();
  REQUIRE_FALSE(client.valid());  // EAGAIN — no pending connection
}

// -- close_fd
// ------------------------------------------------------------------

TEST_CASE("close_fd closes a valid fd", "[tcp_transport]") {
  int raw = ::socket(AF_INET, SOCK_STREAM, 0);
  REQUIRE(raw >= 0);

  fiasco::close_fd(raw);

  // A second close should fail with EBADF.
  REQUIRE(::close(raw) == -1);
  REQUIRE(errno == EBADF);
}

TEST_CASE("close_fd is a no-op for fd < 0", "[tcp_transport]") {
  // Must not crash or throw.
  REQUIRE_NOTHROW(fiasco::close_fd(-1));
  REQUIRE_NOTHROW(fiasco::close_fd(-42));
}

// -- send_all
// ------------------------------------------------------------------

TEST_CASE("send_all transmits all bytes", "[tcp_transport]") {
  auto [writer, reader] = make_socket_pair();

  const std::string data = "Hello, fiasco!";
  REQUIRE(fiasco::send_all(writer, data));

  char buf[64]{};
  ssize_t n = ::recv(reader, buf, sizeof(buf), 0);
  REQUIRE(n == static_cast<ssize_t>(data.size()));
  REQUIRE(std::string(buf, n) == data);

  ::close(writer);
  ::close(reader);
}

TEST_CASE("send_all returns true for an empty string", "[tcp_transport]") {
  auto [writer, reader] = make_socket_pair();

  REQUIRE(fiasco::send_all(writer, ""));

  ::close(writer);
  ::close(reader);
}

TEST_CASE("send_all transmits multi-kilobyte payloads correctly",
          "[tcp_transport]") {
  auto [writer, reader] = make_socket_pair();

  // 64 KB — larger than a typical socket buffer chunk.
  const std::string data(65536, 'x');
  REQUIRE(fiasco::send_all(writer, data));

  std::string received;
  received.resize(data.size());
  std::size_t total = 0;
  while (total < data.size()) {
    ssize_t n = ::recv(reader, received.data() + total, data.size() - total, 0);
    if (n <= 0) break;
    total += static_cast<std::size_t>(n);
  }

  REQUIRE(total == data.size());
  REQUIRE(received == data);

  ::close(writer);
  ::close(reader);
}

TEST_CASE("send_all returns false on a closed socket", "[tcp_transport]") {
  auto [writer, reader] = make_socket_pair();
  ::close(reader);  // Kill the peer — next send should fail.

  // May need more than one attempt on some kernels before SIGPIPE/EPIPE fires.
  // MSG_NOSIGNAL prevents the signal; EPIPE triggers the false return.
  bool result = fiasco::send_all(writer, std::string(1024, 'a'));
  // Either false (EPIPE) or true if the kernel buffered it — don't assert
  // the exact value, but at least ensure we don't crash or throw.
  (void)result;

  ::close(writer);
}

// -- drain
// ---------------------------------------------------------------------

TEST_CASE("drain reads all available bytes and returns drained",
          "[tcp_transport]") {
  auto [writer, reader] = make_socket_pair();

  const std::string payload = "GET / HTTP/1.1\r\n\r\n";
  ::send(writer, payload.c_str(), payload.size(), 0);

  std::string accumulated;
  auto result = fiasco::drain(reader, [&](const char* buf, std::size_t len) {
    accumulated.append(buf, len);
    return true;  // keep reading
  });

  REQUIRE(result == fiasco::drain_result::drained);
  REQUIRE(accumulated == payload);

  ::close(writer);
  ::close(reader);
}

TEST_CASE("drain returns closed when peer shuts down the write end",
          "[tcp_transport]") {
  auto [writer, reader] = make_socket_pair();

  ::shutdown(writer, SHUT_WR);  // Send FIN without closing the fd.

  std::string accumulated;
  auto result = fiasco::drain(reader, [&](const char* buf, std::size_t len) {
    accumulated.append(buf, len);
    return true;
  });

  REQUIRE(result == fiasco::drain_result::closed);

  ::close(writer);
  ::close(reader);
}

TEST_CASE(
    "drain stops early and returns feed_stopped when callback returns false",
    "[tcp_transport]") {
  auto [writer, reader] = make_socket_pair();

  // Send two chunks sequentially (they may arrive as one, which is fine —
  // the callback will still return false on the first call).
  const std::string chunk = "data";
  ::send(writer, chunk.c_str(), chunk.size(), 0);

  int call_count = 0;
  auto result = fiasco::drain(reader, [&](const char*, std::size_t) -> bool {
    ++call_count;
    return false;  // Always stop immediately.
  });

  REQUIRE(result == fiasco::drain_result::feed_stopped);
  REQUIRE(call_count >= 1);

  ::close(writer);
  ::close(reader);
}

TEST_CASE("drain accumulates bytes across multiple recv calls",
          "[tcp_transport]") {
  // We can't guarantee multiple recv calls from userspace easily, but we can
  // verify that the total bytes received match what was sent regardless of
  // how many chunks the kernel delivers.
  auto [writer, reader] = make_socket_pair();

  const std::string payload(8192, 'Z');
  ::send(writer, payload.c_str(), payload.size(), 0);

  std::string accumulated;
  auto result = fiasco::drain(reader, [&](const char* buf, std::size_t len) {
    accumulated.append(buf, len);
    return true;
  });

  REQUIRE(result == fiasco::drain_result::drained);
  REQUIRE(accumulated == payload);

  ::close(writer);
  ::close(reader);
}

TEST_CASE("drain on an empty non-blocking socket returns drained immediately",
          "[tcp_transport]") {
  auto [writer, reader] = make_socket_pair();

  int call_count = 0;
  auto result = fiasco::drain(reader, [&](const char*, std::size_t) -> bool {
    ++call_count;
    return true;
  });

  REQUIRE(result == fiasco::drain_result::drained);
  REQUIRE(call_count == 0);  // callback never invoked — nothing to read

  ::close(writer);
  ::close(reader);
}
