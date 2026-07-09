#pragma once

#include <algorithm>
#include <expected>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

#include "AwaiterBase.hpp"
#include "Coroutine.hpp"

namespace Omni::Fiber {

class ISelectablePair {
public:
  explicit ISelectablePair() = default;
  virtual ~ISelectablePair() = default;

  ISelectablePair(const ISelectablePair&) = delete;
  auto operator=(const ISelectablePair&) -> ISelectablePair& = delete;
  ISelectablePair(ISelectablePair&&) = delete;
  auto operator=(ISelectablePair&&) -> ISelectablePair& = delete;

  virtual void setup_awaiter() = 0;
  virtual auto check_ready() -> bool = 0;
  virtual void setup_suspend(Fiber& fiber) = 0;
  virtual auto run_callback_if_ready() -> Coroutine<void> = 0;
};

template <typename Awaitable, typename Callback> class SelectablePairImpl : public ISelectablePair {
private:
  using AwaitableStorage = std::conditional_t<std::is_reference_v<Awaitable>, Awaitable, std::decay_t<Awaitable>>;
  using CallbackStorage = std::decay_t<Callback>;

  using Awaiter = decltype(std::move(std::declval<AwaitableStorage&>()).operator co_await());
  using AwaiterTraitsType = AwaiterTraits<Awaiter>;
  using AwaiterResultType = typename AwaiterTraitsType::AwaiterResultType;
  using AwaiterResultExpectedType = typename AwaiterTraitsType::AwaiterResultExpectedType;

  AwaitableStorage _awaitable;
  CallbackStorage _callback;
  std::unique_ptr<Awaiter> _awaiter;

public:
  SelectablePairImpl(auto&& awaitable, auto&& callback)
      : _awaitable(std::forward<decltype(awaitable)>(awaitable)),
        _callback(std::forward<decltype(callback)>(callback)) {}

  void setup_awaiter() override { _awaiter.reset(new Awaiter(std::move(_awaitable).operator co_await())); }

  auto check_ready() -> bool override { return _awaiter->await_ready(); }

  void setup_suspend(Fiber& fiber) override {
    _awaiter->SetOwnerPromise(fiber);
    _awaiter->OnAwaitSuspend();
  }

  auto run_callback_if_ready() -> Coroutine<void> override {
    AwaiterResultExpectedType result;
    if (_awaiter->await_ready()) {
      if constexpr (std::is_void_v<AwaiterResultType>) {
        _awaiter->await_resume();
        result = {};
      } else {
        result = _awaiter->await_resume();
      }
    } else {
      result = std::unexpected(typename AwaiterTraitsType::AwaiterNotReady{});
    }

    if constexpr (std::is_void_v<AwaiterResultType>) {
      if (result.has_value()) {
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

    [[nodiscard]] auto await_ready() const -> bool {
      for (auto& pair : _list._pairs) {
        pair->setup_awaiter();
      }
      return std::ranges::any_of(_list._pairs, [](auto& pair) -> auto { return pair->check_ready(); });
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
  using AwaiterResultExpectedType = std::expected<void, typename AwaiterTraits<ListAwaiter>::AwaiterNotReady>;
  using ResultType = std::expected<void, typename AwaiterTraits<ListAwaiter>::AwaiterNotReady>;

  auto operator co_await() -> ListAwaiter { return ListAwaiter(*this); }

  operator Awaiter() { return Awaiter(*this); }

  auto RunCallback(AwaiterResultExpectedType& result) -> Coroutine<ResultType> {
    if (result.has_value()) {
      co_await DoSelect();
      co_return ResultType{};
    }
    co_return std::unexpected(typename AwaiterTraits<ListAwaiter>::AwaiterNotReady{});
  }

private:
  auto DoSelect() -> Coroutine<void> {
    co_await *this;
    for (auto& pair : _pairs) {
      co_await pair->run_callback_if_ready();
    }
  }

  std::vector<std::shared_ptr<ISelectablePair>> _pairs;
};

} // namespace Omni::Fiber
