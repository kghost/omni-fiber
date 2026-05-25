#pragma once

#include <cassert>
#include <coroutine>
#include <exception>
#include <optional>
#include <type_traits>

namespace Omni {
namespace Fiber {

template <typename RetType> class Coroutine {
private:
  template <typename Impl> class PromiseBase {
  public:
    Coroutine get_return_object(this Impl& self) { return {std::coroutine_handle<Impl>::from_promise(self)}; }
    std::suspend_never initial_suspend() const noexcept { return {}; }
    auto final_suspend() noexcept {
      struct FinalAwaiter {
        bool await_ready() noexcept { return false; }
        std::coroutine_handle<> await_suspend(std::coroutine_handle<Impl> h) noexcept {
          if (h.promise()._Caller.has_value()) {
            return h.promise()._Caller.value();
          } else {
            return std::noop_coroutine();
          }
        }
        void await_resume() noexcept {}
      };
      return FinalAwaiter{};
    }
    void unhandled_exception() { _Exception.emplace(std::current_exception()); }

    bool IsFinished() const noexcept { return _Exception.has_value(); }

  protected:
    friend class Coroutine;
    std::optional<std::exception_ptr> _Exception;
    std::optional<std::coroutine_handle<>> _Caller;
  };

  class PromiseVoid final : public PromiseBase<PromiseVoid> {
  public:
    void return_void() { _IsReturned = true; }

    bool IsFinished() const noexcept { return PromiseBase<PromiseVoid>::IsFinished() || _IsReturned; }

  private:
    friend class Coroutine;
    bool _IsReturned = false;
  };

  class PromiseNonVoid final : public PromiseBase<PromiseNonVoid> {
  public:
    void return_value(RetType&& ret) { _ReturnValue.emplace(std::move(ret)); }

    bool IsFinished() const noexcept { return PromiseBase<PromiseNonVoid>::IsFinished() || _ReturnValue.has_value(); }
    RetType&& GetReturnValue() { return std::move(_ReturnValue.value()); }

  private:
    friend class Coroutine;
    std::optional<RetType> _ReturnValue;
  };

public:
  using promise_type = std::conditional<std::is_void_v<RetType>, PromiseVoid, PromiseNonVoid>::type;
  Coroutine(std::coroutine_handle<promise_type> callee) : _Callee(callee) {}
  ~Coroutine() {
    assert(_Callee.promise().IsFinished()); // If hits, you probably forget to co_await a Coroutine.
    _Callee.destroy();
  }

  Coroutine(const Coroutine&) = delete;
  Coroutine& operator=(const Coroutine&) = delete;
  Coroutine(const Coroutine&&) = delete;
  Coroutine& operator=(const Coroutine&&) = delete;

  Coroutine& operator co_await() { return *this; }

  bool await_ready() const noexcept { return _Callee.promise().IsFinished(); }
  template <typename T> void await_suspend(std::coroutine_handle<T> caller) noexcept {
    _Callee.promise()._Caller.emplace(caller);
  }
  RetType await_resume() {
    promise_type& promise = _Callee.promise();
    if (promise._Exception.has_value()) {
      // This throws the exception directly into the caller's execution frame
      std::rethrow_exception(promise._Exception.value());
    }

    if constexpr (std::is_void_v<RetType>) {
      return;
    } else {
      return _Callee.promise().GetReturnValue();
    }
  }

private:
  std::coroutine_handle<promise_type> _Callee;
};

} // namespace Fiber
} // namespace Omni
