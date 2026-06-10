#pragma once

#include <memory>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

#include "AwaiterBase.hpp"
#include "Coroutine.hpp"

namespace Omni {
namespace Fiber {

class ISelectablePair {
public:
  virtual ~ISelectablePair() = default;
  virtual void setup_awaiter() = 0;
  virtual bool check_ready() = 0;
  virtual void setup_suspend(Fiber& fiber) = 0;
  virtual Coroutine<void> run_callback_if_ready() = 0;
};

template <typename Awaitable, typename Callback> class SelectablePairImpl : public ISelectablePair {
private:
  using AwaitableStorage = std::conditional_t<std::is_reference_v<Awaitable>, Awaitable, std::decay_t<Awaitable>>;
  using CallbackStorage = std::decay_t<Callback>;

  using Awaiter = decltype(std::move(std::declval<AwaitableStorage&>()).operator co_await());
  using AwaiterTraitsType = AwaiterTraits<Awaiter>;
  using AwaiterResultType = typename AwaiterTraitsType::AwaiterResultType;
  using AwaiterResultOptionalType = typename AwaiterTraitsType::AwaiterResultOptionalType;

  AwaitableStorage _awaitable;
  CallbackStorage _callback;
  std::unique_ptr<Awaiter> _awaiter;

public:
  SelectablePairImpl(Awaitable&& awaitable, Callback&& callback)
      : _awaitable(std::forward<Awaitable>(awaitable)), _callback(std::forward<Callback>(callback)) {}

  void setup_awaiter() override { _awaiter.reset(new Awaiter(std::move(_awaitable).operator co_await())); }

  bool check_ready() override { return _awaiter->await_ready(); }

  void setup_suspend(Fiber& fiber) override {
    _awaiter->SetOwnerPromise(fiber);
    _awaiter->OnAwaitSuspend();
  }

  Coroutine<void> run_callback_if_ready() override {
    AwaiterResultOptionalType result;
    if (_awaiter->await_ready()) {
      if constexpr (std::is_void_v<AwaiterResultType>) {
        _awaiter->await_resume();
        result = true;
      } else {
        result = _awaiter->await_resume();
      }
    } else {
      if constexpr (std::is_void_v<AwaiterResultType>) {
        result = false;
      } else {
        result = std::nullopt;
      }
    }

    if constexpr (std::is_same_v<std::decay_t<decltype(result)>, bool>) {
      if (result) {
        if constexpr (requires { typename decltype(_callback())::CoroutineReturnType; }) {
          co_await _callback();
        } else {
          _callback();
        }
      }
    } else {
      if (result.has_value()) {
        auto&& val = std::move(*result);
        if constexpr (requires { typename decltype(_callback(std::move(val)))::CoroutineReturnType; }) {
          co_await _callback(std::move(val));
        } else {
          _callback(std::move(val));
        }
      }
    }
    co_return;
  }
};

class SelectPairDynamicList {
public:
  SelectPairDynamicList() = default;

  template <typename Awaitable, typename Callback> void Add(Awaitable&& awaitable, Callback&& callback) {
    _pairs.push_back(std::make_shared<SelectablePairImpl<Awaitable, Callback>>(std::forward<Awaitable>(awaitable),
                                                                               std::forward<Callback>(callback)));
  }

  struct ListAwaiter : public AwaiterBase<FiberSuspender> {
    SelectPairDynamicList& _list;
    explicit ListAwaiter(SelectPairDynamicList& list) : _list(list) {}

    bool await_ready() const {
      for (auto& pair : _list._pairs) {
        pair->setup_awaiter();
      }
      for (auto& pair : _list._pairs) {
        if (pair->check_ready()) {
          return true;
        }
      }
      return false;
    }

    void OnAwaitSuspend() {
      auto& parent = this->GetOwnerPromise();
      for (auto& pair : _list._pairs) {
        pair->setup_suspend(parent);
      }
    }

    template <typename PromiseType> void await_suspend(std::coroutine_handle<PromiseType> caller) {
      this->DoAwaitSuspend(caller);
      OnAwaitSuspend();
    }

    void await_resume() {}
  };

  using Awaiter = ListAwaiter;
  using AwaiterResultOptionalType = bool;
  using ResultType = bool;

  ListAwaiter operator co_await() { return ListAwaiter(*this); }

  operator Awaiter() { return Awaiter(*this); }

  Coroutine<bool> RunCallback(bool result) {
    if (result) {
      co_await DoSelect();
      co_return true;
    }
    co_return false;
  }

private:
  Coroutine<void> DoSelect() {
    co_await *this;
    for (auto& pair : _pairs) {
      co_await pair->run_callback_if_ready();
    }
  }

  std::vector<std::shared_ptr<ISelectablePair>> _pairs;
};

} // namespace Fiber
} // namespace Omni
