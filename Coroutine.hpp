#pragma once

#include <cassert>
#include <coroutine>
#include <exception>
#include <expected>
#include <functional>
#include <optional>
#include <type_traits>

#include "FiberPromise.hpp"

#ifndef NDEBUG
#include <boost/log/common.hpp>
#include <boost/log/trivial.hpp>
#endif

namespace Omni {
namespace Fiber {

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
    void unhandled_exception(this Impl& self) {
      self._RetState = std::unexpected(std::current_exception());
#ifndef NDEBUG
      self.SetInstructionPointer(__builtin_return_address(0));
      auto& fiber = self.GetFiber();
      fiber.SetSuspendedPromise(&self);
      boost::log::sources::severity_logger<boost::log::trivial::severity_level> logger;
      BOOST_LOG_SEV(logger, boost::log::trivial::severity_level::error) << "Unhandled exception escaping coroutine:";
      fiber.DumpCallStack(logger, 0);
#endif
    }
    bool IsFinished() const noexcept { return _RetState.has_value(); }

  protected:
    std::optional<std::expected<RetType, std::exception_ptr>> _RetState;
    std::optional<std::coroutine_handle<>> _Caller;
    std::optional<std::reference_wrapper<FiberPromise>> _CallerPromise;
  };

  class PromiseVoid final : public PromiseBase<PromiseVoid> {
  public:
    void return_void() { this->_RetState.emplace(); }
    friend class Coroutine;
  };

  class PromiseNonVoid final : public PromiseBase<PromiseNonVoid> {
  public:
    template <typename T> void return_value(T&& ret) { this->_RetState = std::forward<T>(ret); }
    friend class Coroutine;
  };

public:
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
  template <typename T> std::coroutine_handle<> await_suspend(std::coroutine_handle<T> caller) noexcept {
    promise_type& promise = _Callee.promise();
    promise._Caller.emplace(caller);
    promise._CallerPromise.emplace(caller.promise());
#ifndef NDEBUG
    caller.promise().SetInstructionPointer(__builtin_return_address(0));
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

} // namespace Fiber
} // namespace Omni
