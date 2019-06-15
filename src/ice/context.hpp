#pragma once
#include <atomic>
#include <condition_variable>
#include <experimental/coroutine>
#include <mutex>
#include <thread>

namespace ice {

class context {
public:
  class event {
  public:
    event() noexcept = default;

#ifdef __INTELLISENSE__
    // clang-format off
    event(const event& other) noexcept {}
    event& operator=(const event& other) noexcept {}
    // clang-format on
#else
    event(const event& other) = delete;
    event& operator=(const event& other) = delete;
#endif

    virtual ~event() = default;

    void resume() noexcept {
      awaiter_.resume();
    }

  protected:
    std::experimental::coroutine_handle<> awaiter_;

  private:
    friend class context;
    std::atomic<event*> next_ = nullptr;
  };

  context() = default;

  context(const context& other) = delete;
  context& operator=(const context& other) = delete;

  void run() noexcept {
    thread_.store(std::this_thread::get_id(), std::memory_order_release);
    std::unique_lock lock{ mutex_ };
    lock.unlock();
    while (true) {
      lock.lock();
      auto head = head_.exchange(nullptr, std::memory_order_acquire);
      while (!head) {
        if (stop_.load(std::memory_order_acquire)) {
          lock.unlock();
          return;
        }
        cv_.wait(lock, []() { return true; });
        head = head_.exchange(nullptr, std::memory_order_acquire);
      }
      lock.unlock();
      while (head) {
        auto next = head->next_.load(std::memory_order_relaxed);
        head->resume();
        head = next;
      }
    }
  }

  bool is_current() const noexcept {
    return thread_.load(std::memory_order_acquire) == std::this_thread::get_id();
  }

  void stop() noexcept {
    stop_.store(true, std::memory_order_release);
    cv_.notify_all();
  }

  void schedule(event* ev) noexcept {
    auto head = head_.load(std::memory_order_acquire);
    do {
      ev->next_.store(head, std::memory_order_relaxed);
    } while (!head_.compare_exchange_weak(head, ev, std::memory_order_release, std::memory_order_acquire));
    cv_.notify_one();
  }

private:
  std::atomic_bool stop_ = false;
  std::atomic<event*> head_ = nullptr;
  std::atomic<std::thread::id> thread_;
  std::condition_variable cv_;
  std::mutex mutex_;
};

class schedule final : public context::event {
public:
  schedule(context& context, bool post = false) noexcept : context_(context), ready_(!post && context.is_current()) {
  }

  constexpr bool await_ready() const noexcept {
    return ready_;
  }

  void await_suspend(std::experimental::coroutine_handle<> awaiter) noexcept {
    awaiter_ = awaiter;
    context_.schedule(this);
  }

  constexpr void await_resume() const noexcept {
  }

private:
  context& context_;
  const bool ready_ = true;
};

}  // namespace ice
