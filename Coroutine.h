#pragma once

#include <cassert>
#include <coroutine>
#include <exception>
#include <functional>
#include <optional>
#include <type_traits>
#include <variant>

#include "FiberPromise.hpp"

namespace Omni {
namespace Fiber {

template <typename RetType> class Coroutine {
private:
  template <typename Impl> class PromiseBase : public FiberPromise {
  public:
    Fiber& GetFiber() override { return _CallerPromise.value().get().GetFiber(); }
    Manager& GetManager() override { return _CallerPromise.value().get().GetManager(); }

    Coroutine get_return_object(this Impl& self) { return {std::coroutine_handle<Impl>::from_promise(self)}; }
    std::suspend_always initial_suspend() const noexcept { return {}; }
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
    void unhandled_exception() { _RetState = std::current_exception(); }
    bool IsFinished() const noexcept { return _RetState.index() != 0; }

  protected:
    using RetDataType = std::conditional_t<std::is_void_v<RetType>, bool, RetType>;
    std::variant<std::monostate, std::exception_ptr, RetDataType> _RetState = std::monostate{};
    std::optional<std::coroutine_handle<>> _Caller;
    std::optional<std::reference_wrapper<FiberPromise>> _CallerPromise;
  };

  class PromiseVoid final : public PromiseBase<PromiseVoid> {
  public:
    void return_void() { this->_RetState = true; }
    friend class Coroutine;
  };

  class PromiseNonVoid final : public PromiseBase<PromiseNonVoid> {
  public:
    void return_value(RetType&& ret) { this->_RetState = std::move(ret); }
    RetType&& GetReturnValue() { return std::move(std::get<2>(this->_RetState)); }
    friend class Coroutine;
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
  template <typename T> std::coroutine_handle<> await_suspend(std::coroutine_handle<T> caller) noexcept {
    promise_type& promise = _Callee.promise();
    promise._Caller.emplace(caller);
    promise._CallerPromise.emplace(caller.promise());
    return _Callee;
  }
  RetType await_resume() {
    promise_type& promise = _Callee.promise();
    if (promise._RetState.index() == 1) {
      // This throws the exception directly into the caller's execution frame
      std::rethrow_exception(std::get<std::exception_ptr>(promise._RetState));
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
