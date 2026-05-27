#pragma once

#include <algorithm>
#include <cassert>
#include <coroutine>
#include <exception>
#include <memory>
#include <optional>
#include <set>
#include <type_traits>

#include <boost/describe/enum.hpp>
#include <boost/log/common.hpp>
#include <boost/log/trivial.hpp>

#include "Coroutine.hpp"
#include "FiberFinishNotifier.hpp"
#include "FiberPromise.hpp"
#include "SharedAwaitable.hpp"

#include "shared.h"

namespace Omni {
namespace Fiber {

class Manager;
class SingleAwaitable;

class Fiber : public std::enable_shared_from_this<Fiber> {
private:
  class FiberFrame {
  public:
    class Promise final : public FiberPromise {
    public:
      explicit Promise(Fiber& fiber, auto&) : _Fiber(fiber) {}

      Promise(const Promise&) = delete;
      Promise& operator=(const Promise&) = delete;
      Promise(Promise&&) = delete;
      Promise& operator=(Promise&&) = delete;

      Fiber& GetFiber() override { return _Fiber; }

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
          void await_suspend(std::coroutine_handle<> /*unused*/) const noexcept { _Fiber.Finishing(); }
          constexpr void await_resume() const noexcept {}

        private:
          Fiber& _Fiber;
        };
        return Awaitor(_Fiber);
      }

      void unhandled_exception() { _Fiber.SetException(std::current_exception()); }
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
    explicit FiberFrame(std::coroutine_handle<> state) : _CoroutineState(state) {}
    std::coroutine_handle<> _CoroutineState;
  };

public:
  template <typename CoroutineFunction>
    requires std::is_invocable_r_v<Coroutine<void>, CoroutineFunction>
  std::shared_ptr<Fiber> Spawn(std::string name, CoroutineFunction&& function) {
    assert(std::ranges::none_of(_Children, [&](auto& e) { return e->GetName() == name; }));
    auto child = std::shared_ptr<Fiber>(
        new Fiber(_Manager, std::move(name), _ChildFiberFinishNotifier, std::forward<CoroutineFunction>(function)));
    _Children.insert(child);
    child->Schedule();
    return child;
  }

  enum class State { NotStart, Running, Suspending, Suspended, Ready, Finishing, Finished };
  BOOST_DESCRIBE_NESTED_ENUM(State, NotStart, Running, Suspending, Suspended, Ready, Finishing, Finished);

  OMNIFIBER_API const std::string& GetName() const { return _Name; }
  OMNIFIBER_API bool IsFinished() { return _State == State::Finished; }
  OMNIFIBER_API void Schedule();
  OMNIFIBER_API Coroutine<void> Wait(std::function<bool()> until);
  OMNIFIBER_API Coroutine<void> Join(std::shared_ptr<Fiber> child);
  OMNIFIBER_API Coroutine<std::shared_ptr<Fiber>> WaitFor();
  OMNIFIBER_API Coroutine<void> WaitAll();

  void DumpAllFibers(boost::log::sources::severity_logger<boost::log::trivial::severity_level>& logger, int indent);

private:
  friend class Manager;
  friend class SharedAwaitable;
  friend class SingleAwaitable;
  friend boost::log::formatting_ostream& operator<<(boost::log::formatting_ostream& p, Fiber& fiber);

  class ChildFiberFinishNotifier : public FiberFinishNotifier {
  public:
    ChildFiberFinishNotifier(Fiber& parent) : _Parent(parent) {}
    ~ChildFiberFinishNotifier() override {}
    void OnFiberFinished(std::shared_ptr<Fiber> fiber) override { _Parent.OnChildFinished(fiber); }

  private:
    Fiber& _Parent;
  };

  // owner is used by FiberFrame::Promise constructor
  template <typename CoroutineFunction> static FiberFrame SpawnFiber(Fiber& /*unused*/, CoroutineFunction function) {
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

  OMNIFIBER_API Manager& GetManager() { return _Manager; }
  OMNIFIBER_API void Suspend(std::coroutine_handle<> caller);
  OMNIFIBER_API void Resume(); // Called by Manager to continue this fiber.

  OMNIFIBER_API void StartingYield(std::coroutine_handle<> caller);
  OMNIFIBER_API void Finishing();
  OMNIFIBER_API void SetException(std::exception_ptr eptr);
  void OnChildFinished(std::shared_ptr<Fiber> child);

  Manager& _Manager;
  const std::string _Name;
  FiberFinishNotifier& _FinishNotifier;
  ChildFiberFinishNotifier _ChildFiberFinishNotifier;

  State _State = State::NotStart;

  // Continuation state
  std::optional<std::coroutine_handle<>> _Continuation;

  // Reture state
  std::optional<std::exception_ptr> _Exception;

  // This field must initialized later than _Continuation, becasue _Continuation is wroten when initializing Frame
  FiberFrame _OutMostFrame;

  // Children management.
  std::set<std::shared_ptr<Fiber>> _Children;
  std::set<std::shared_ptr<Fiber>> _FinishedChildren;
  SharedAwaitable::ContextStorage _JoinAwaitContext;
};

boost::log::formatting_ostream& operator<<(boost::log::formatting_ostream& p, Fiber& fiber);

} // namespace Fiber
} // namespace Omni
