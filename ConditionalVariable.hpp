#pragma once

#include <cassert>
#include <coroutine>
#include <list>
#include <optional>

#include <cstdint>
#include <utility>

#include "AwaiterBase.hpp"
#include "Coroutine.hpp"
#include "Mutex.hpp"

namespace Omni::Fiber {

class ConditionalVariable final {
public:
  explicit ConditionalVariable() = default;
  ~ConditionalVariable() = default;

  ConditionalVariable(const ConditionalVariable&) = delete;
  auto operator=(const ConditionalVariable&) -> ConditionalVariable& = delete;
  ConditionalVariable(ConditionalVariable&&) = delete;
  auto operator=(ConditionalVariable&&) -> ConditionalVariable& = delete;

  class WaitAwaiter : public AwaiterBase<FiberSuspender> {
  public:
    explicit WaitAwaiter(ConditionalVariable& condVar, Mutex& mutex) : _Cv(condVar), _Mutex(mutex) {}
    ~WaitAwaiter() {
      if (this->IsSuspended()) {
        if (_State == WaitState::InCvQueue && _CvIt.has_value()) {
          _Cv._WaitList.erase(*_CvIt);
        } else if (_State == WaitState::InMutexQueue && _MutexIt.has_value()) {
          if (!(*_MutexIt)->acquired) {
            _Mutex._WaitList.erase(*_MutexIt);
          }
        }
      }
    }

    WaitAwaiter(const WaitAwaiter&) = delete;
    auto operator=(const WaitAwaiter&) -> WaitAwaiter& = delete;
    WaitAwaiter(WaitAwaiter&&) = delete;
    auto operator=(WaitAwaiter&&) -> WaitAwaiter& = delete;

    [[nodiscard]] auto await_ready() const noexcept -> bool {
      if (_State == WaitState::Acquired) {
        return true;
      }
      if (_State == WaitState::InMutexQueue && _MutexIt.has_value() && (*_MutexIt)->acquired) {
        return true;
      }
      return false;
    }

    template <typename PromiseType> void await_suspend(std::coroutine_handle<PromiseType> caller) {
      DoAwaitSuspend(caller);
      OnAwaitSuspend();
    }

    void OnAwaitSuspend() {
      _CvIt = _Cv._WaitList.insert(_Cv._WaitList.end(), this);
      _State = WaitState::InCvQueue;
      _Mutex.Unlock();
    }

    constexpr void await_resume() const noexcept {}

  private:
    friend class ConditionalVariable;

    enum class WaitState : std::uint8_t {
      None,
      InCvQueue,
      InMutexQueue,
      Acquired
    };

    void ReacquireMutex() {
      _State = WaitState::None;
      if (!_Mutex._Locked) {
        _Mutex._Locked = true;
        _State = WaitState::Acquired;
        this->Schedule();
      } else {
        _MutexIt = _Mutex._WaitList.insert(
            _Mutex._WaitList.end(),
            Mutex::WaitNode{._Fiber = &this->GetOwnerPromise(), .acquired = false});
        _State = WaitState::InMutexQueue;
      }
    }

    ConditionalVariable& _Cv;
    Mutex& _Mutex;
    WaitState _State = WaitState::None;
    std::optional<typename std::list<WaitAwaiter*>::iterator> _CvIt;
    std::optional<typename std::list<Mutex::WaitNode>::iterator> _MutexIt;
  };

  [[nodiscard]] auto Wait(Mutex& mutex) -> WaitAwaiter { return WaitAwaiter(*this, mutex); }

  template <typename Predicate>
  auto Wait(Mutex& mutex, Predicate&& pred) -> Coroutine<void> {
    while (!std::forward<Predicate>(pred)()) {
      co_await Wait(mutex);
    }
  }

  void NotifyOne() {
    if (!_WaitList.empty()) {
      auto* awaiter = _WaitList.front();
      _WaitList.pop_front();
      awaiter->ReacquireMutex();
    }
  }

  void NotifyAll() {
    while (!_WaitList.empty()) {
      auto* awaiter = _WaitList.front();
      _WaitList.pop_front();
      awaiter->ReacquireMutex();
    }
  }

private:
  std::list<WaitAwaiter*> _WaitList;
};

} // namespace Omni::Fiber
