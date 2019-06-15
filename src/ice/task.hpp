// https://github.com/lewissbaker/cppcoro/blob/67fea7fe53284a62f95900b6f56fe6dbd69e4d5f/include/cppcoro/task.hpp
//
// Copyright 2017 Lewis Baker
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is furnished
// to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once
#include <atomic>
#include <functional>
#include <type_traits>
#include <utility>
#include <cassert>

#include <experimental/coroutine>

namespace ice {

template <typename T>
class task;

namespace detail {

class continuation {
public:
  using callback_t = void(void*);

  constexpr continuation() noexcept = default;

  explicit continuation(std::experimental::coroutine_handle<> awaiter) noexcept : callback_(nullptr), state_(awaiter.address()) {
  }

  explicit constexpr continuation(callback_t* callback, void* state) noexcept : callback_(callback), state_(state) {
  }

  explicit constexpr operator bool() const noexcept {
    return callback_ || state_;
  }

  void resume() noexcept {
    if (callback_) {
      callback_(state_);
    } else {
      std::experimental::coroutine_handle<>::from_address(state_).resume();
    }
  }

private:
  callback_t* callback_{ nullptr };
  void* state_{ nullptr };
};

class task_promise_base {
public:
  constexpr task_promise_base() noexcept = default;

  constexpr auto initial_suspend() noexcept {
    return std::experimental::suspend_never{};
  }

  auto final_suspend() noexcept {
    struct awaitable {
      constexpr awaitable(task_promise_base& promise) noexcept : promise_(promise) {
      }

      bool await_ready() const noexcept {
        return promise_.state_.load(std::memory_order_acquire) == state::consumer_detached;
      }

      bool await_suspend(std::experimental::coroutine_handle<>) noexcept {
        state state = promise_.state_.exchange(state::finished, std::memory_order_acq_rel);
        if (state == state::consumer_suspended) {
          promise_.continuation_.resume();
        }
        return state != state::consumer_detached;
      }

      constexpr void await_resume() noexcept {
      }

      task_promise_base& promise_;
    };

    return awaitable{ *this };
  }

#ifndef _MSC_VER
  void unhandled_exception() noexcept {
    assert(false);
  }
#endif

  bool is_ready() const noexcept {
    return state_.load(std::memory_order_acquire) == state::finished;
  }

  bool try_detach() noexcept {
    return state_.exchange(state::consumer_detached, std::memory_order_acq_rel) == state::running;
  }

  bool try_await(detail::continuation continuation) {
    continuation_ = continuation;
    state state = state::running;
    return state_.compare_exchange_strong(state, state::consumer_suspended, std::memory_order_release, std::memory_order_acquire);
  }

private:
  enum class state { running, consumer_suspended, consumer_detached, finished };

  std::atomic<state> state_{ state::running };
  detail::continuation continuation_;
};

template <typename T>
class task_promise : public task_promise_base {
public:
  constexpr task_promise() noexcept = default;

  ~task_promise() {
    reinterpret_cast<T*>(&value_)->~T();
  }

  task<T> get_return_object() noexcept;

  template <typename... Args>
  void return_value(Args&&... args) noexcept {
    new (&value_) T(std::forward<Args>(args)...);
  }

  constexpr T& result() & noexcept {
    return *reinterpret_cast<T*>(&value_);
  }

  constexpr T&& result() && noexcept {
    return std::move(*reinterpret_cast<T*>(&value_));
  }

private:
  alignas(T) char value_[sizeof(T)];
};

template <>
class task_promise<void> : public task_promise_base {
public:
  constexpr task_promise() noexcept = default;

  task<void> get_return_object() noexcept;

  constexpr void return_void() noexcept {
  }

  constexpr void result() noexcept {
  }
};

template <typename T>
class task_promise<T&> : public task_promise_base {
public:
  constexpr task_promise() noexcept = default;

  task<T&> get_return_object() noexcept;

  void return_value(T& value) noexcept {
    value_ = std::addressof(value);
  }

  constexpr T& result() noexcept {
    return *value_;
  }

private:
  T* value_;
};

}  // namespace detail

template <typename T = void>
class task {
public:
  using promise_type = detail::task_promise<T>;
  using value_type = T;

private:
  struct awaitable_base {
    awaitable_base(std::experimental::coroutine_handle<promise_type> coroutine) noexcept : coroutine_(coroutine) {
    }

    bool await_ready() const noexcept {
      return !coroutine_ || coroutine_.promise().is_ready();
    }

    bool await_suspend(std::experimental::coroutine_handle<> awaiter) noexcept {
      return coroutine_.promise().try_await(detail::continuation{ awaiter });
    }

    std::experimental::coroutine_handle<promise_type> coroutine_;
  };

public:
  constexpr task() noexcept = default;

  explicit constexpr task(std::experimental::coroutine_handle<promise_type> coroutine) : coroutine_(coroutine) {
  }

  constexpr task(task&& task) noexcept : coroutine_(task.coroutine_) {
    task.coroutine_ = nullptr;
  }

  task(const task&) = delete;
  task& operator=(const task&) = delete;

  ~task() {
    destroy();
  }

  task& operator=(task&& other) noexcept {
    if (std::addressof(other) != this) {
      destroy();
      coroutine_ = other.coroutine_;
      other.coroutine_ = nullptr;
    }
    return *this;
  }

  bool is_ready() const noexcept {
    return !coroutine_ || coroutine_.promise().is_ready();
  }

  void detach() noexcept {
    if (coroutine_) {
      auto coro = coroutine_;
      coroutine_ = nullptr;
      if (!coro.promise().try_detach()) {
        coro.destroy();
      }
    }
  }

  auto operator co_await() const& noexcept {
    struct awaitable : awaitable_base {
      using awaitable_base::awaitable_base;
      decltype(auto) await_resume() noexcept {
        assert(this->coroutine_);
        return this->coroutine_.promise().result();
      }
    };
    return awaitable{ coroutine_ };
  }

  auto operator co_await() const&& noexcept {
    struct awaitable : awaitable_base {
      using awaitable_base::awaitable_base;
      decltype(auto) await_resume() noexcept {
        assert(this->coroutine_);
        return std::move(this->coroutine_.promise()).result();
      }
    };
    return awaitable{ coroutine_ };
  }

  auto when_ready() const noexcept {
    struct awaitable : awaitable_base {
      using awaitable_base::awaitable_base;
      constexpr void await_resume() const noexcept {
      }
    };

    return awaitable{ coroutine_ };
  }

  auto get_starter() const noexcept {
    class starter {
    public:
      constexpr starter(std::experimental::coroutine_handle<promise_type> coroutine) noexcept : coroutine_(coroutine) {
      }

      void start(detail::continuation continuation) noexcept {
        if (!coroutine_ || coroutine_.promise().is_ready() || !coroutine_.promise().try_await(continuation)) {
          continuation.resume();
        }
      }

    private:
      std::experimental::coroutine_handle<promise_type> coroutine_;
    };

    return starter{ coroutine_ };
  }

private:
  void destroy() noexcept {
    if (coroutine_) {
      if (!coroutine_.promise().is_ready()) {
        std::terminate();
      }
      coroutine_.destroy();
    }
  }

  std::experimental::coroutine_handle<promise_type> coroutine_{ nullptr };
};

template <typename T>
task<T> detail::task_promise<T>::get_return_object() noexcept {
  return task<T>{ std::experimental::coroutine_handle<task_promise<T>>::from_promise(*this) };
}

template <typename T>
task<T&> detail::task_promise<T&>::get_return_object() noexcept {
  return task<T&>{ std::experimental::coroutine_handle<task_promise<T&>>::from_promise(*this) };
}

inline task<void> detail::task_promise<void>::get_return_object() noexcept {
  return task<void>{ std::experimental::coroutine_handle<task_promise<void>>::from_promise(*this) };
}

}  // namespace ice
