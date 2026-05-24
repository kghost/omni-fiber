#pragma once

#include <cassert>
#include <coroutine>
#include <exception>
#include <memory>
#include <optional>
#include <set>
#include <stdexcept>
#include <type_traits>

#include <boost/describe/enum.hpp>
#include <boost/log/common.hpp>
#include <boost/log/trivial.hpp>

#include "Coroutine.h"
#include "EventQueue.h"
#include "FiberFinishNotifier.h"

#include "shared.h"

namespace Omni {
namespace Fiber {

class Manager;

class Fiber : public std::enable_shared_from_this<Fiber> {
private:
  class FiberFrame {
  public:
    class Promise {
    public:
      Promise(Fiber& fiber, auto&) : _Fiber(fiber) {}

      Promise(Promise&&) = delete;
      Promise(const Promise&) = delete;

      auto initial_suspend() const noexcept {
        class Awaitor {
        public:
          Awaitor(Fiber& owner) : _Fiber(owner) {}
          constexpr bool await_ready() const noexcept { return false; }
          void await_suspend(std::coroutine_handle<> caller) const noexcept { _Fiber.StartingYield(caller); }
          constexpr void await_resume() const noexcept {}

        private:
          Fiber& _Fiber;
        };
        return Awaitor(_Fiber);
      }

      auto final_suspend() const noexcept {
        class Awaitor {
        public:
          Awaitor(Fiber& owner) : _Fiber(owner) {}
          constexpr bool await_ready() const noexcept { return false; }
          void await_suspend(std::coroutine_handle<> caller) const noexcept { _Fiber.Finishing(); }
          constexpr void await_resume() const noexcept {}

        private:
          Fiber& _Fiber;
        };
        return Awaitor(_Fiber);
      }

      void unhandled_exception() { _Fiber.SetException(std::current_exception()); } // TODO
      FiberFrame get_return_object() { return FiberFrame(std::coroutine_handle<promise_type>::from_promise(*this)); }
      void return_void() {}

    private:
      Fiber& _Fiber;
    };

    using promise_type = Promise;

    void operator co_await() = delete;

    FiberFrame(const FiberFrame&) = delete;
    FiberFrame& operator=(const FiberFrame&) = delete;
    FiberFrame(FiberFrame&& that) = delete;
    FiberFrame& operator=(FiberFrame&&) = delete;

    ~FiberFrame() { _CoroutineState.destroy(); }

  private:
    FiberFrame(std::coroutine_handle<> state) : _CoroutineState(state) {}
    std::coroutine_handle<> _CoroutineState;
  };

public:
  template <typename CoroutineFunction>
    requires std::is_invocable_r_v<Coroutine<void>, CoroutineFunction>
  std::shared_ptr<Fiber> Spawn(std::string&& name, CoroutineFunction&& function) {
    auto child = std::shared_ptr<Fiber>(
        new Fiber(_Manager, std::move(name), _ChildFiberFinishNotifier, std::forward<CoroutineFunction>(function)));
    _Children.insert(child);
    child->Schedule();
    return child;
  }

  enum class State { NotStart, Running, Suspending, Suspended, Ready, Finishing, Finished };
  BOOST_DESCRIBE_NESTED_ENUM(State, NotStart, Running, Suspending, Suspended, Ready, Finishing, Finished);

  OMNIFIBER_API bool IsFinished() { return _State == State::Finished; }
  OMNIFIBER_API void Interrupt() { _Interrupted = true; } // Insert FiberInterrupted exception at next suspend point.
  OMNIFIBER_API void Schedule();
  OMNIFIBER_API Coroutine<void> Join(std::shared_ptr<Fiber> child); // Join the child fiber.

  class FiberInterrupted : public std::runtime_error {
  public:
    FiberInterrupted() : std::runtime_error("Fiber Interrupted.") {}
  };

  void DumpAllFibers(boost::log::sources::severity_logger<boost::log::trivial::severity_level>& logger, int indent);

private:
  friend class Manager;
  friend class FiberAwaitable;
  friend boost::log::formatting_ostream& operator<<(boost::log::formatting_ostream& p, Fiber& fiber);

  class ChildFiberFinishNotifier : public FiberFinishNotifier {
  public:
    ChildFiberFinishNotifier(Fiber& fiber) : _Parent(fiber) {}
    ~ChildFiberFinishNotifier() override {}
    void OnFiberFinished(std::shared_ptr<Fiber> fiber) override { _Parent._ChildSignals.Push(fiber); }

  private:
    Fiber& _Parent;
  };

  // owner is used by FiberFrame::Promise constructor
  template <typename CoroutineFunction> static FiberFrame SpawnFiber(Fiber& owner, CoroutineFunction function) {
    co_await function();
  }

  template <typename CoroutineFunction>
    requires std::is_invocable_r_v<Coroutine<void>, CoroutineFunction>
  Fiber(Manager& manager, std::string&& name, FiberFinishNotifier& notifier, CoroutineFunction&& function)
      : _Manager(manager), _Name(std::move(name)), _FinishNotifier(notifier), _ChildFiberFinishNotifier(*this),
        _OutMostFrame(SpawnFiber(*this, std::forward<CoroutineFunction>(function))) {}

  Fiber(const Fiber&) = delete;
  Fiber& operator=(const Fiber&) = delete;
  Fiber(Fiber&&) = delete;
  Fiber& operator=(Fiber&&) = delete;

  OMNIFIBER_API void Suspend(std::coroutine_handle<> caller);
  OMNIFIBER_API void Resume(); // Called by Manager to schedule this fiber.

  OMNIFIBER_API void StartingYield(std::coroutine_handle<> caller);
  OMNIFIBER_API void Finishing();
  OMNIFIBER_API void SetException(std::exception_ptr eptr);

  Manager& _Manager;
  const std::string _Name;
  FiberFinishNotifier& _FinishNotifier;
  ChildFiberFinishNotifier _ChildFiberFinishNotifier;

  State _State = State::NotStart;

  // Continuation state
  std::optional<std::coroutine_handle<>> _Continuation;
  bool _Interrupted = false;

  // Reture state
  std::optional<std::exception_ptr> _Exception;

  FiberFrame _OutMostFrame; // This field must initialized later than _Continuation, becasue _Continuation is wroten
                            // when initializing Frame
  EventQueue<std::shared_ptr<Fiber>> _ChildSignals;
  std::set<std::shared_ptr<Fiber>> _Children;
};

boost::log::formatting_ostream& operator<<(boost::log::formatting_ostream& p, Fiber& fiber);

} // namespace Fiber
} // namespace Omni
