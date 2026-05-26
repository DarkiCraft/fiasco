#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <future>
#include <thread>

#include "fiasco/core/thread_pool.hpp"

TEST_CASE("thread_pool constructs with default thread count", "[thread_pool]") {
  fiasco::detail::thread_pool pool;
  REQUIRE(pool.size() > 0);
}

TEST_CASE("thread_pool constructs with explicit thread count",
          "[thread_pool]") {
  fiasco::detail::thread_pool pool(4);
  REQUIRE(pool.size() == 4);
}

TEST_CASE("thread_pool executes submitted tasks", "[thread_pool]") {
  fiasco::detail::thread_pool pool(2);
  std::atomic<int> counter{0};

  for (int i = 0; i < 100; ++i) {
    pool.submit([&counter] { counter.fetch_add(1); });
  }

  pool.shutdown();
  REQUIRE(counter.load() == 100);
}

TEST_CASE("thread_pool shutdown is idempotent", "[thread_pool]") {
  fiasco::detail::thread_pool pool(2);
  pool.shutdown();
  pool.shutdown();  // Should not crash or hang
}

TEST_CASE("thread_pool rejects tasks after shutdown", "[thread_pool]") {
  fiasco::detail::thread_pool pool(2);
  pool.shutdown();

  std::atomic<bool> ran{false};
  pool.submit([&ran] { ran.store(true); });

  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  REQUIRE_FALSE(ran.load());
}

// -- try_submit tests ---------------------------------------------------------

TEST_CASE("try_submit returns true when queue has capacity", "[thread_pool]") {
  fiasco::detail::thread_pool pool(2, /*max_queue=*/10);
  REQUIRE(pool.try_submit([] {}));
}

TEST_CASE("try_submit returns false after shutdown", "[thread_pool]") {
  fiasco::detail::thread_pool pool(2, 10);
  pool.shutdown();
  REQUIRE_FALSE(pool.try_submit([] {}));
}

TEST_CASE("try_submit returns false when queue is at capacity",
          "[thread_pool]") {
  // Single worker, queue capacity 2.
  fiasco::detail::thread_pool pool(1, /*max_queue=*/2);

  // Block the sole worker with a task that waits for our signal.
  // shared_future makes the lambda copyable so it fits in std::function.
  std::promise<void> release;
  std::shared_future<void> gate = release.get_future().share();
  pool.submit([gate] { gate.wait(); });

  // Give the worker time to dequeue and start the blocking task.
  std::this_thread::sleep_for(std::chrono::milliseconds(30));

  // Fill the queue to its capacity of 2.
  REQUIRE(pool.try_submit([] {}));
  REQUIRE(pool.try_submit([] {}));

  // Queue is full — this submission must be rejected.
  REQUIRE_FALSE(pool.try_submit([] {}));

  // Unblock the worker so the pool destructor can join cleanly.
  release.set_value();
}

// -- queue_size tests ---------------------------------------------------------

TEST_CASE("queue_size reflects number of pending tasks", "[thread_pool]") {
  // Single worker so new tasks queue up rather than execute immediately.
  fiasco::detail::thread_pool pool(1, /*max_queue=*/100);

  // Hold the sole worker.
  std::promise<void> release;
  std::shared_future<void> gate = release.get_future().share();
  pool.submit([gate] { gate.wait(); });

  std::this_thread::sleep_for(std::chrono::milliseconds(30));

  // Enqueue two more tasks while the worker is blocked.
  pool.submit([] {});
  pool.submit([] {});

  REQUIRE(pool.queue_size() == 2);

  // Release; destructor drains and joins.
  release.set_value();
}
