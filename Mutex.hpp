#pragma once

#include <cassert>
#include <coroutine>
#include <list>
#include <optional>

#include "AwaiterBase.hpp"
#include "Fiber.hpp"

namespace Omni::Fiber {

class ConditionalVariable;

class Mutex final {
public:
  explicit Mutex() = default;
  ~Mutex() = default;

  Mutex(const Mutex&) = delete;
  auto operator=(const Mutex&) -> Mutex& = delete;
  Mutex(Mutex&&) = delete;
  auto operator=(Mutex&&) -> Mutex& = delete;

  [[nodiscard]] auto IsLocked() const noexcept -> bool { return _Locked; }

  class LockAwaiter : public AwaiterBase<FiberSuspender> {
  public:
    explicit LockAwaiter(Mutex& mutex) : _Mutex(mutex) {}
    ~LockAwaiter() {
      if (this->IsSuspended() && _It.has_value()) {
        if (!(*_It)->acquired) {
          _Mutex._WaitList.erase(*_It);
        }
      }
    }

    LockAwaiter(const LockAwaiter&) = delete;
    auto operator=(const LockAwaiter&) -> LockAwaiter& = delete;
    LockAwaiter(LockAwaiter&&) = delete;
    auto operator=(LockAwaiter&&) -> LockAwaiter& = delete;

    [[nodiscard]] auto await_ready() const noexcept -> bool {
      if (_It.has_value() && (*_It)->acquired) {
        return true;
      }
      if (!_Mutex._Locked) {
        _Mutex._Locked = true;
        return true;
      }
      return false;
    }

    template <typename PromiseType> void await_suspend(std::coroutine_handle<PromiseType> caller) {
      DoAwaitSuspend(caller);
      OnAwaitSuspend();
    }

    void OnAwaitSuspend() {
      _It = _Mutex._WaitList.insert(_Mutex._WaitList.end(),
                                     WaitNode{._Fiber = &this->GetOwnerPromise(), .acquired = false});
    }

    constexpr void await_resume() const noexcept {}

  private:
    Mutex& _Mutex;
    struct WaitNode {
      Fiber* _Fiber = nullptr;
      bool acquired = false;
    };
    friend class Mutex;
    friend class ConditionalVariable;
    std::optional<typename std::list<WaitNode>::iterator> _It;
  };

  auto operator co_await() -> LockAwaiter { return LockAwaiter(*this); }
  [[nodiscard]] auto Lock() -> LockAwaiter { return LockAwaiter(*this); }

  void Unlock() {
    assert(_Locked && "Unlocking a mutex that is not locked");
    for (auto& node : _WaitList) {
      if (!node.acquired) {
        node.acquired = true;
        node._Fiber->Schedule();
        return;
      }
    }
    _Locked = false;
  }

private:
  using WaitNode = LockAwaiter::WaitNode;
  friend class ConditionalVariable;

  bool _Locked = false;
  std::list<WaitNode> _WaitList;
};

} // namespace Omni::Fiber
