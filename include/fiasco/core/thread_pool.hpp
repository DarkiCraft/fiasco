#ifndef FIASCO_THREAD_POOL_HPP
#define FIASCO_THREAD_POOL_HPP

/// @file thread_pool.hpp
/// @brief Lightweight thread pool for handler execution.

#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace fiasco {
/// @brief A simple fixed-size thread pool.
///
/// Tasks are submitted via submit() and executed by worker threads.
/// The pool joins all threads on destruction.
class thread_pool {
 public:
  /// @brief Creates a thread pool with the given number of workers.
  /// @param num_threads  Number of worker threads (default: hardware
  /// concurrency).
  /// @param max_queue    Maximum number of pending tasks. 0 means unbounded.
  explicit thread_pool(
      unsigned int num_threads = std::thread::hardware_concurrency(),
      std::size_t max_queue = 0)
      : m_stop(false), m_max_queue(max_queue) {
    if (num_threads == 0) {
      num_threads = 2;  // Sane fallback
    }

    m_workers.reserve(num_threads);
    for (unsigned int i = 0; i < num_threads; ++i) {
      m_workers.emplace_back([this] { worker_loop(); });
    }
  }

  ~thread_pool() { shutdown(); }

  // Non-copyable, non-movable
  thread_pool(const thread_pool&) = delete;
  thread_pool& operator=(const thread_pool&) = delete;
  thread_pool(thread_pool&&) = delete;
  thread_pool& operator=(thread_pool&&) = delete;

  /// @brief Submits a task for execution. Always enqueues if there is capacity
  ///        or no capacity limit. Drops the task silently after shutdown.
  ///
  /// Thread-safe. Prefer try_submit() when backpressure matters.
  void submit(std::function<void()> task) {
    {
      std::lock_guard<std::mutex> lock(m_mutex);
      if (m_stop) {
        return;  // Reject tasks after shutdown
      }
      m_tasks.push(std::move(task));
    }
    m_cv.notify_one();
  }

  /// @brief Tries to submit a task. Returns false if the pool is stopped or
  ///        the queue is at capacity (backpressure signal to the caller).
  ///
  /// Thread-safe.
  bool try_submit(std::function<void()> task) {
    {
      std::lock_guard lock(m_mutex);
      if (m_stop) {
        return false;
      }
      if (m_max_queue > 0 && m_tasks.size() >= m_max_queue) {
        return false;
      }
      m_tasks.push(std::move(task));
    }
    m_cv.notify_one();
    return true;
  }

  /// @brief Signals all workers to stop and joins them.
  ///
  /// Any remaining queued tasks will still be executed before shutdown.
  void shutdown() {
    {
      std::lock_guard lock(m_mutex);
      if (m_stop) {
        return;  // Already shut down
      }
      m_stop = true;
    }
    m_cv.notify_all();

    for (auto& worker : m_workers) {
      if (worker.joinable()) {
        worker.join();
      }
    }
  }

  /// @brief Returns the number of worker threads.
  [[nodiscard]] std::size_t size() const noexcept { return m_workers.size(); }

  /// @brief Returns the current number of pending tasks.
  [[nodiscard]] std::size_t queue_size() const noexcept {
    std::lock_guard lock(m_mutex);
    return m_tasks.size();
  }

 private:
  void worker_loop() {
    while (true) {
      std::function<void()> task;
      {
        std::unique_lock lock(m_mutex);
        m_cv.wait(lock, [this] { return m_stop || !m_tasks.empty(); });

        if (m_stop && m_tasks.empty()) {
          return;
        }

        task = std::move(m_tasks.front());
        m_tasks.pop();
      }
      task();
    }
  }

  std::vector<std::thread> m_workers;
  std::queue<std::function<void()>> m_tasks;
  mutable std::mutex m_mutex;
  std::condition_variable m_cv;
  bool m_stop;
  std::size_t m_max_queue;  // 0 = unbounded
};

}  // namespace fiasco

#endif  // FIASCO_THREAD_POOL_HPP
