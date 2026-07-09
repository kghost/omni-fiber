#pragma once

#include <cassert>
#include <coroutine>
#include <exception>
#include <expected>
#include <functional>
#include <optional>
#include <type_traits>

#if __has_include(<stacktrace>)
#include <stacktrace>
#endif

#include "FiberPromise.hpp"

namespace Omni::Fiber {

#ifndef NDEBUG
void DebugOutputFiberCallStack(Fiber& fiber, FiberPromise& promise, const std::exception_ptr& eptr);
#endif

template <typename RetType> class [[nodiscard]] Coroutine {
private:
  template <typename Impl> class PromiseBase : public FiberPromise {
  public:
    auto GetFiber() -> Fiber& override { return _CallerPromise.value().get().GetFiber(); }

#ifndef NDEBUG
    [[nodiscard]] auto GetCallerPromise() const noexcept -> FiberPromise* override {
      return _CallerPromise.has_value() ? &_CallerPromise.value().get() : nullptr;
    }
#endif

    auto get_return_object(this Impl& self) -> Coroutine {
      return Coroutine{std::coroutine_handle<Impl>::from_promise(self)};
    }
    [[nodiscard]] auto initial_suspend() const noexcept -> std::suspend_always { return {}; }
    auto final_suspend() noexcept {
      struct FinalAwaiter {
        auto await_ready() noexcept -> bool { return false; }
        auto await_suspend(std::coroutine_handle<Impl> self) noexcept -> std::coroutine_handle<> {
          assert(self.promise()._CallerPromise.has_value());
          return self.promise()._CallerPromise.value().get().GetCoroutineHandle();
        }
        void await_resume() noexcept {}
      };
      return FinalAwaiter{};
    }
    void unhandled_exception(
        this Impl& self
#ifndef NDEBUG
#if __has_include(<stacktrace>)
        ,
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast, performance-no-int-to-ptr)
        void* instructionPointer = reinterpret_cast<void*>(std::stacktrace::current(0, 1).at(0).native_handle())
#endif
#endif
    ) {
      self._RetState = std::unexpected(std::current_exception());
#ifndef NDEBUG
#if __has_include(<stacktrace>)
      self.SetInstructionPointer(instructionPointer);
#else
      self.SetInstructionPointer(__builtin_return_address(0));
#endif
      DebugOutputFiberCallStack(self.GetFiber(), self, self._RetState.value().error());
#endif
    }
    [[nodiscard]] auto IsFinished() const noexcept -> bool { return _RetState.has_value(); }

  protected:
    std::optional<std::expected<RetType, std::exception_ptr>> _RetState;
    std::optional<std::reference_wrapper<FiberPromise>> _CallerPromise;
  };

  class PromiseVoid final : public PromiseBase<PromiseVoid> {
  public:
    auto GetCoroutineHandle() noexcept -> std::coroutine_handle<> override {
      return std::coroutine_handle<PromiseVoid>::from_promise(*this);
    }
    void return_void() { this->_RetState.emplace(); }
    friend class Coroutine;
  };

  class PromiseNonVoid final : public PromiseBase<PromiseNonVoid> {
  public:
    auto GetCoroutineHandle() noexcept -> std::coroutine_handle<> override {
      return std::coroutine_handle<PromiseNonVoid>::from_promise(*this);
    }
    template <typename T> void return_value(T&& ret) {
      this->_RetState = typename decltype(this->_RetState)::value_type(std::in_place, std::forward<T>(ret));
    }
    friend class Coroutine;
  };

public:
  using CoroutineReturnType = RetType;
  using promise_type = std::conditional_t<std::is_void_v<RetType>, PromiseVoid, PromiseNonVoid>;
  explicit Coroutine(std::coroutine_handle<promise_type> callee) : _Callee(callee) {}
  ~Coroutine() {
    assert(_Callee.promise().IsFinished() && "you probably forgot to co_await a Coroutine.");
    _Callee.destroy();
  }

  Coroutine(const Coroutine&) = delete;
  auto operator=(const Coroutine&) -> Coroutine& = delete;
  Coroutine(const Coroutine&&) = delete;
  auto operator=(const Coroutine&&) -> Coroutine& = delete;

  auto operator co_await() -> Coroutine& { return *this; }

  [[nodiscard]] auto await_ready() const noexcept -> bool { return _Callee.promise().IsFinished(); }
  template <typename T>
  auto
  await_suspend(std::coroutine_handle<T> caller
#ifndef NDEBUG
#if __has_include(<stacktrace>)
                ,
                // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast, performance-no-int-to-ptr)
                void* instructionPointer = reinterpret_cast<void*>(std::stacktrace::current(0, 1).at(0).native_handle())
#endif
#endif
                    ) noexcept -> std::coroutine_handle<> {
    promise_type& promise = _Callee.promise();
    promise._CallerPromise.emplace(caller.promise());
#ifndef NDEBUG
#if __has_include(<stacktrace>)
    caller.promise().SetInstructionPointer(instructionPointer);
#else
    caller.promise().SetInstructionPointer(__builtin_return_address(0));
#endif
#endif
    return _Callee;
  }
  auto await_resume() -> RetType {
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

} // namespace Omni::Fiber
