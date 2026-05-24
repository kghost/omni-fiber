#pragma once

#include <cassert>
#include <coroutine>
#include <exception>
#include <memory>
#include <optional>
#include <queue>
#include <type_traits>

namespace Omni {
namespace Fiber {

template <typename RetType> class Coroutine {
private:
  class ReturnState {
  public:
    void FinishWithValue() noexcept requires std::is_void_v<RetType> { _RetValue = true; }
    template <typename RetType = RetType>
    requires !std::is_void_v<RetType> void FinishWithValue(RetType && ret) noexcept {
      _RetValue.emplace(std::move(ret));
    }
    void FinishWithException(std::exception_ptr eptr) noexcept { _Exception = eptr; }

    bool IsFinished() const noexcept { return bool(_RetValue) || _Exception != nullptr; }

    RetType Continue() {
      assert(IsFinished());
      if (_Exception != nullptr)
        std::rethrow_exception(_Exception);
      if constexpr (std::is_void_v<RetType>)
        return;
      else
        return std::move(_RetValue.value());
    }

    bool HasWaiter() const noexcept { return _Continuation.has_value(); }
    void SetWaiter(std::coroutine_handle<> waiter) noexcept { _Continuation.emplace(waiter); }
    void ResumeWaiter() const noexcept { return _Continuation.value().resume(); }

  private:
    std::optional<std::coroutine_handle<>> _Continuation;
    std::conditional_t<std::is_void_v<RetType>, bool, std::optional<RetType>> _RetValue = ([] {
      if constexpr (std::is_void_v<RetType>)
        return false;
      else
        return std::optional<RetType>();
    })();
    std::exception_ptr _Exception;
  };

public:
  class PromiseBase {
  public:
    auto initial_suspend() const noexcept { return std::suspend_never{}; }
    auto final_suspend() noexcept {
      // It is suggested to always suspend at final_suspend. Such that the Coroutine object controls the lifespan of the
      // coroutine state. The Coroutine inside the Corotine::Awaitor object controls when the Coroutine is destroyed.
      class FinalAwaiter {
      public:
        FinalAwaiter(ReturnState &ret) : _ReturnState(ret) {}

        bool await_ready() const noexcept { return false; }
        void await_suspend(std::coroutine_handle<>) const noexcept {
          // If the Coroutine is ever suspended, resume upper frame, otherwise return the control to upper frame.
          if (_ReturnState.HasWaiter())
            _ReturnState.ResumeWaiter();
        }
        void await_resume() const noexcept {}

      private:
        ReturnState &_ReturnState;
      };

      return FinalAwaiter(*_ReturnState);
    }

    void unhandled_exception() { std::terminate(); } // TODO

  protected:
    std::shared_ptr<ReturnState> _ReturnState;
  };

  class PromiseVoid : public PromiseBase {
  public:
    Coroutine get_return_object() {
      Coroutine coroutine(std::coroutine_handle<promise_type>::from_promise(*this));
      PromiseBase::_ReturnState = coroutine._RetState;
      return coroutine;
    }

    void return_void() { PromiseBase::_ReturnState->FinishWithValue(); }
  };

  class PromiseNonVoid : public PromiseBase {
  public:
    Coroutine get_return_object() {
      Coroutine coroutine(std::coroutine_handle<promise_type>::from_promise(*this));
      PromiseBase::_ReturnState = coroutine._RetState;
      return coroutine;
    }

    void return_value(RetType &&ret) { PromiseBase::_ReturnState->FinishWithValue(std::move(ret)); }
  };

  using promise_type = std::conditional<std::is_void_v<RetType>, PromiseVoid, PromiseNonVoid>::type;

  // The Awaitor is stored in the caller's coroutine state, its template parameter RetType is the callee's return type.
  // It will recevie the caller's coroutine_handle at await_suspend.
  //
  // When a caller Coroutine awaits an inner Coroutine, the Coroutine of inner callee becomes the Awaitable, stored
  // inside the Coroutine of outer caller. When the callee finished, it will resume the outer caller via await_resume.
  //
  // co_await a Coroutine doesn't change the state of the fiber.
  //
  // The lifespan of coroutine state of the callee is bound to _Callee.
  class Awaitable {
  public:
    Awaitable(Coroutine &&callee) : _Callee(std::move(callee)) {}

    bool await_ready() const noexcept { return _Callee._RetState->IsFinished(); }
    void await_suspend(std::coroutine_handle<> caller) noexcept { _Callee._RetState->SetWaiter(caller); }
    RetType await_resume() noexcept { return _Callee._RetState->Continue(); }

  private:
    Coroutine _Callee;
  };

  auto operator co_await() { return Awaitable(std::move(*this)); }

  Coroutine(const Coroutine &) = delete;
  Coroutine &operator=(const Coroutine &) = delete;

  Coroutine(Coroutine &&that)
      : _RetState(std::move(that._RetState)), _CoroutineState(std::move(that._CoroutineState.value())) {
    that._CoroutineState.reset();
  }
  Coroutine &operator=(Coroutine &&) = delete;

  ~Coroutine() {
    assert(!_RetState || _RetState->IsFinished()); // If hits, you probably forget to co_await a Coroutine.
    if (_CoroutineState.has_value())
      _CoroutineState.value().destroy();
  }

private:
  Coroutine(std::coroutine_handle<> state) : _CoroutineState(state) {}

  std::shared_ptr<ReturnState> _RetState = std::make_shared<ReturnState>();
  std::optional<std::coroutine_handle<>> _CoroutineState;
};

} // namespace Fiber
} // namespace Omni
