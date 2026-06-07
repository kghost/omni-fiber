#pragma once

#include <cassert>
#include <coroutine>
#include <exception>
#include <expected>
#include <functional>
#include <optional>
#include <stacktrace>
#include <type_traits>

#include "FiberPromise.hpp"

namespace Omni {
namespace Fiber {

#ifndef NDEBUG
void DebugOutputFiberCallStack(Fiber& fiber, FiberPromise& promise, std::exception_ptr eptr);
#endif

template <typename RetType> class [[nodiscard]] Coroutine {
private:
  template <typename Impl> class PromiseBase : public FiberPromise {
  public:
    Fiber& GetFiber() override { return _CallerPromise.value().get().GetFiber(); }

#ifndef NDEBUG
    FiberPromise* GetCallerPromise() const noexcept override {
      return _CallerPromise.has_value() ? &_CallerPromise.value().get() : nullptr;
    }
#endif

    Coroutine get_return_object(this Impl& self) { return Coroutine{std::coroutine_handle<Impl>::from_promise(self)}; }
    std::suspend_always initial_suspend() const noexcept { return {}; }
    auto final_suspend() noexcept {
      struct FinalAwaiter {
        bool await_ready() noexcept { return false; }
        std::coroutine_handle<> await_suspend(std::coroutine_handle<Impl> self) noexcept {
          assert(self.promise()._CallerPromise.has_value());
          return self.promise()._CallerPromise.value().get().GetCoroutineHandle();
        }
        void await_resume() noexcept {}
      };
      return FinalAwaiter{};
    }
    void unhandled_exception(this Impl& self
#ifndef NDEBUG
                             ,
                             void* ip = (void*)std::stacktrace::current().at(0).native_handle()
#endif
    ) {
      self._RetState = std::unexpected(std::current_exception());
#ifndef NDEBUG
      self.SetInstructionPointer(ip);
      DebugOutputFiberCallStack(self.GetFiber(), self, self._RetState.value().error());
#endif
    }
    bool IsFinished() const noexcept { return _RetState.has_value(); }

  protected:
    std::optional<std::expected<RetType, std::exception_ptr>> _RetState;
    std::optional<std::reference_wrapper<FiberPromise>> _CallerPromise;
  };

  class PromiseVoid final : public PromiseBase<PromiseVoid> {
  public:
    std::coroutine_handle<> GetCoroutineHandle() noexcept override {
      return std::coroutine_handle<PromiseVoid>::from_promise(*this);
    }
    void return_void() { this->_RetState.emplace(); }
    friend class Coroutine;
  };

  class PromiseNonVoid final : public PromiseBase<PromiseNonVoid> {
  public:
    std::coroutine_handle<> GetCoroutineHandle() noexcept override {
      return std::coroutine_handle<PromiseNonVoid>::from_promise(*this);
    }
    template <typename T> void return_value(T&& ret) {
      this->_RetState = typename decltype(this->_RetState)::value_type(std::in_place, std::forward<T>(ret));
    }
    friend class Coroutine;
  };

public:
  using CoroutineReturnType = RetType;
  using promise_type = std::conditional<std::is_void_v<RetType>, PromiseVoid, PromiseNonVoid>::type;
  explicit Coroutine(std::coroutine_handle<promise_type> callee) : _Callee(callee) {}
  ~Coroutine() {
    assert(_Callee.promise().IsFinished() && "you probably forgot to co_await a Coroutine.");
    _Callee.destroy();
  }

  Coroutine(const Coroutine&) = delete;
  Coroutine& operator=(const Coroutine&) = delete;
  Coroutine(const Coroutine&&) = delete;
  Coroutine& operator=(const Coroutine&&) = delete;

  Coroutine& operator co_await() { return *this; }

  bool await_ready() const noexcept { return _Callee.promise().IsFinished(); }
  template <typename T>
  std::coroutine_handle<> await_suspend(std::coroutine_handle<T> caller
#ifndef NDEBUG
                                        ,
                                        void* ip = (void*)std::stacktrace::current().at(0).native_handle()
#endif
                                            ) noexcept {
    promise_type& promise = _Callee.promise();
    promise._CallerPromise.emplace(caller.promise());
#ifndef NDEBUG
    caller.promise().SetInstructionPointer(ip);
#endif
    return _Callee;
  }
  RetType await_resume() {
    promise_type& promise = _Callee.promise();
    auto& ret = promise._RetState.value();
    if (ret.has_value()) {
      if constexpr (std::is_void_v<RetType>) {
        return;
      } else {
        return std::move(ret.value());
      }
    } else {
      std::rethrow_exception(ret.error());
    }
  }

private:
  std::coroutine_handle<promise_type> _Callee;
};

template <typename T> struct CoroutineTraits : std::false_type {
  using CoroutineReturnTypeOrOriginalType = T;
};
template <typename Arg> struct CoroutineTraits<Coroutine<Arg>> : std::true_type {
  using CoroutineReturnTypeOrOriginalType = typename Coroutine<Arg>::CoroutineReturnType;
};

} // namespace Fiber
} // namespace Omni
