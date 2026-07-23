#pragma once

#include <cassert>
#include <coroutine>
#include <functional>
#include <list>
#include <memory>
#include <optional>

#include "AwaiterBase.hpp"
#include "Coroutine.hpp"

namespace Omni::Fiber {

class LockGuard;

class Mutex final {
public:
  explicit Mutex() = default;
  ~Mutex() = default;

  Mutex(const Mutex&) = delete;
  auto operator=(const Mutex&) -> Mutex& = delete;
  Mutex(Mutex&&) = delete;
  auto operator=(Mutex&&) -> Mutex& = delete;

  [[nodiscard]] auto IsLocked() const noexcept -> bool { return _LockOwner.has_value(); }

  auto Wait() -> Omni::Fiber::Coroutine<std::unique_ptr<class LockGuard>>;

private:
  void ScheduleNext();

  class Awaiter : public AwaiterBase<FiberSuspender> {
  public:
    explicit Awaiter(Mutex& mutex) : _Mutex(mutex), _It(_Mutex._WaitList.insert(_Mutex._WaitList.end(), *this)) {}
    ~Awaiter() { _Mutex._WaitList.erase(*_It); }

    Awaiter(const Awaiter&) = delete;
    auto operator=(const Awaiter&) -> Awaiter& = delete;
    Awaiter(Awaiter&& other) noexcept = delete;
    auto operator=(Awaiter&&) -> Awaiter& = delete;

    [[nodiscard]] auto await_ready() const noexcept -> bool { return !_Mutex._LockOwner.has_value(); }

    template <typename PromiseType> void await_suspend(std::coroutine_handle<PromiseType> caller) {
      DoAwaitSuspend(caller);
    }

    auto await_resume() const noexcept -> void {}

  private:
    Mutex& _Mutex;
    std::optional<typename std::list<std::reference_wrapper<Awaiter>>::iterator> _It;
  };

  friend class LockGuard;
  friend class Awaiter;

  std::optional<std::reference_wrapper<Omni::Fiber::LockGuard>> _LockOwner;
  std::list<std::reference_wrapper<Awaiter>> _WaitList;
};

class LockGuard final {
public:
  explicit LockGuard(Mutex& mutex) noexcept : _Mutex(mutex) {
    assert(!mutex._LockOwner.has_value() && "Locking a mutex that is already locked");
    mutex._LockOwner = std::ref(*this);
  }

  ~LockGuard() {
    assert(_Mutex._LockOwner.has_value() && std::addressof(_Mutex._LockOwner.value().get()) == this &&
           "Unlocking a mutex that is not owned by this LockGuard");
    _Mutex._LockOwner.reset();
    _Mutex.ScheduleNext();
  }

  LockGuard(const LockGuard&) = delete;
  auto operator=(const LockGuard&) -> LockGuard& = delete;
  LockGuard(LockGuard&& other) noexcept = delete;
  auto operator=(LockGuard&& other) noexcept -> LockGuard& = delete;

private:
  Mutex& _Mutex;
};

inline auto Mutex::Wait() -> Omni::Fiber::Coroutine<std::unique_ptr<class LockGuard>> {
  while (IsLocked()) {
    co_await Awaiter(*this);
  }
  co_return std::make_unique<class LockGuard>(*this);
}

inline void Mutex::ScheduleNext() {
  assert(!IsLocked());
  if (_WaitList.empty()) {
    return;
  }
  _WaitList.begin()->get().Schedule();
}

} // namespace Omni::Fiber
