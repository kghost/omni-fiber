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

#include "AwaiterAlwaysSuspend.hpp"
#include "Coroutine.hpp"
#include "FiberFinishNotifier.hpp"
#include "FiberPromise.hpp"
#include "SharedAwaiter.hpp"

namespace Omni::Fiber {

class Manager;
class SharedAwaitContext;

class Fiber {
private:
  class FiberFrame {
  public:
    class Promise final : public FiberPromise {
    public:
      explicit Promise(Fiber& fiber, auto& /*unused*/) : _Fiber(fiber) {}
      ~Promise() = default;

      Promise(const Promise&) = delete;
      auto operator=(const Promise&) -> Promise& = delete;
      Promise(Promise&&) = delete;
      auto operator=(Promise&&) -> Promise& = delete;

      auto GetFiber() -> Fiber& override { return _Fiber; }
      auto GetCoroutineHandle() noexcept -> std::coroutine_handle<> override {
        return std::coroutine_handle<Promise>::from_promise(*this);
      }

      [[nodiscard]] auto initial_suspend() const noexcept {
        class Awaitor {
        public:
          Awaitor(Fiber& owner) : _Fiber(owner) {}
          [[nodiscard]] static constexpr auto await_ready() noexcept -> bool { return false; }
          void await_suspend(std::coroutine_handle<Promise> caller) const noexcept { _Fiber.Starting(caller); }
          constexpr void await_resume() const noexcept {}

        private:
          Fiber& _Fiber;
        };
        return Awaitor(_Fiber);
      }

      [[nodiscard]] auto final_suspend() const noexcept {
        class Awaitor {
        public:
          Awaitor(Fiber& owner) : _Fiber(owner) {}
          [[nodiscard]] static constexpr auto await_ready() noexcept -> bool { return false; }
          void await_suspend(std::coroutine_handle<> /*unused*/) const noexcept { _Fiber.Finishing(); }
          constexpr void await_resume() const noexcept {}

        private:
          Fiber& _Fiber;
        };
        return Awaitor(_Fiber);
      }

      void unhandled_exception() { _Fiber.SetException(std::current_exception()); }
      auto get_return_object() -> FiberFrame {
        return FiberFrame(std::coroutine_handle<promise_type>::from_promise(*this));
      }
      void return_void() {}

    private:
      Fiber& _Fiber;
    };

    using promise_type = Promise;

    void operator co_await() = delete;

    FiberFrame(const FiberFrame&) = delete;
    auto operator=(const FiberFrame&) -> FiberFrame& = delete;
    FiberFrame(FiberFrame&& that) = delete;
    auto operator=(FiberFrame&&) -> FiberFrame& = delete;

    ~FiberFrame() { _CoroutineState.destroy(); }

  private:
    explicit FiberFrame(std::coroutine_handle<> state) : _CoroutineState(state) {}
    std::coroutine_handle<> _CoroutineState;
  };

public:
  template <typename CoroutineFunction>
    requires std::is_invocable_r_v<Coroutine<void>, CoroutineFunction>
  auto Spawn(std::string name, CoroutineFunction&& function) -> std::shared_ptr<Fiber> {
    assert(std::ranges::none_of(_Children, [&](auto& child) -> auto { return child->GetName() == name; }));
    auto child = std::shared_ptr<Fiber>(
        new Fiber(_Manager, std::move(name), _ChildFiberFinishNotifier, std::forward<CoroutineFunction>(function)));
    _Children.insert(child);
    child->Schedule();
    return child;
  }

  enum class State : std::uint8_t {
    NotStart,
    Running,
    Suspending,
    Suspended,
    Yielding,
    Yielded,
    Ready,
    Finishing,
    Finished
  };
  BOOST_DESCRIBE_NESTED_ENUM(State, NotStart, Running, Suspending, Suspended, Yielding, Yielded, Ready, Finishing,
                             Finished);

  [[nodiscard]] auto GetName() const -> const std::string& { return _Name; }
  auto GetManager() -> Manager& { return _Manager; }
  auto IsFinished() -> bool { return _State == State::Finished; }
  void Schedule();

  auto ChildAwaitor() -> AwaiterAlwaysSuspend<SharedAwaiter>;
  auto Wait(std::function<bool()> until) -> Coroutine<void>;
  auto TryJoin(const std::shared_ptr<Fiber>& child) -> bool;
  auto Join(const std::shared_ptr<Fiber>& child) -> Coroutine<void>;
  auto WaitFor() -> Coroutine<std::shared_ptr<Fiber>>;
  auto WaitAll() -> Coroutine<void>;

  void DumpAllFibers(boost::log::sources::severity_logger<boost::log::trivial::severity_level>& logger, int indent);
#ifndef NDEBUG
  void DumpCallStack(boost::log::sources::severity_logger<boost::log::trivial::severity_level>& logger, int indent);
  void SetSuspendedPromise(FiberPromise* suspendedPromise) { _SuspendedPromise = suspendedPromise; }
#else
  void DumpCallStack(boost::log::sources::severity_logger<boost::log::trivial::severity_level>&, int) {}
#endif

private:
  friend class Manager;
  friend struct FiberSuspender;
  friend struct FiberYielder;
  friend auto operator<<(boost::log::formatting_ostream& stream, Fiber& fiber) -> boost::log::formatting_ostream&;

  class ChildFiberFinishNotifier : public FiberFinishNotifier {
  public:
    explicit ChildFiberFinishNotifier(Fiber& parent) : _Parent(parent) {}
    ~ChildFiberFinishNotifier() override {}

    ChildFiberFinishNotifier(const ChildFiberFinishNotifier&) = delete;
    auto operator=(const ChildFiberFinishNotifier&) -> ChildFiberFinishNotifier& = delete;
    ChildFiberFinishNotifier(ChildFiberFinishNotifier&&) = delete;
    auto operator=(ChildFiberFinishNotifier&&) -> ChildFiberFinishNotifier& = delete;

    void OnFiberFinished(Fiber& fiber) override { _Parent.OnChildFinished(fiber); }

  private:
    Fiber& _Parent;
  };

  // owner is used by FiberFrame::Promise constructor
  template <typename CoroutineFunction> static FiberFrame SpawnFiber(Fiber& /*unused*/, CoroutineFunction function) {
    co_await function();
  }

public:
  template <typename CoroutineFunction>
    requires std::is_invocable_r_v<Coroutine<void>, CoroutineFunction>
  Fiber(Manager& manager, std::string&& name, FiberFinishNotifier& notifier, CoroutineFunction&& function)
      : _Manager(manager), _Name(std::move(name)), _FinishNotifier(notifier), _ChildFiberFinishNotifier(*this),
        _OutMostFrame(SpawnFiber(*this, std::forward<CoroutineFunction>(function))) {}
  ~Fiber() = default;

  Fiber(const Fiber&) = delete;
  auto operator=(const Fiber&) -> Fiber& = delete;
  Fiber(Fiber&&) = delete;
  auto operator=(Fiber&&) -> Fiber& = delete;

private:
  void Suspend(std::coroutine_handle<> caller);
  void OmniYield(std::coroutine_handle<> caller);
  void Resume(); // Called by Manager to continue this fiber.

  void Starting(std::coroutine_handle<Fiber::FiberFrame::Promise> caller);
  void Finishing();
  void SetException(const std::exception_ptr& eptr);
  void OnChildFinished(Fiber& child);

  Manager& _Manager;
  const std::string _Name;
  FiberFinishNotifier& _FinishNotifier;
  ChildFiberFinishNotifier _ChildFiberFinishNotifier;

  State _State = State::NotStart;
  std::optional<std::coroutine_handle<>> _Continuation;
  std::optional<std::exception_ptr> _Exception;
  FiberFrame _OutMostFrame;

  // Children management.
  struct Less {
    using is_transparent = void;
    auto operator()(const std::shared_ptr<Fiber>& lhs, const std::shared_ptr<Fiber>& rhs) const -> bool {
      return std::to_address(lhs) < std::to_address(rhs);
    }
    auto operator()(const std::shared_ptr<Fiber>& lhs, const Fiber& rhs) const -> bool {
      return std::to_address(lhs) < std::addressof(rhs);
    }
    auto operator()(const Fiber& lhs, const std::shared_ptr<Fiber>& rhs) const -> bool {
      return std::addressof(lhs) < std::to_address(rhs);
    }
  };

  std::set<std::shared_ptr<Fiber>, Less> _Children;
  std::set<std::shared_ptr<Fiber>, Less> _FinishedChildren;
  std::weak_ptr<SharedAwaitContext> _JoinAwaitContext;

#ifndef NDEBUG
  FiberPromise* _SuspendedPromise = nullptr;
#endif
};

auto operator<<(boost::log::formatting_ostream& stream, Fiber& fiber) -> boost::log::formatting_ostream&;

} // namespace Omni::Fiber
