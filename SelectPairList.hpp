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

template <typename Awaitable, typename Callback> class SelectPairList {
public:
  using AwaitableStorage = std::conditional_t<std::is_reference_v<Awaitable>, Awaitable, std::decay_t<Awaitable>>;
  using CallbackStorage = std::decay_t<Callback>;

  struct Pair {
    AwaitableStorage first;
    CallbackStorage second;
  };

  struct ListAwaiter;
  using Awaiter = ListAwaiter;

  using UnderlyingAwaiter = decltype(std::move(std::declval<AwaitableStorage&>()).operator co_await());
  using AwaiterTraitsType = AwaiterTraits<UnderlyingAwaiter>;
  using AwaiterResultType = typename AwaiterTraitsType::AwaiterResultType;
  using UnderlyingAwaiterResultOptionalType = typename AwaiterTraitsType::AwaiterResultOptionalType;
  using AwaiterResultOptionalType = std::optional<std::vector<UnderlyingAwaiterResultOptionalType>>;

  using CallbackResultType = decltype(([] -> decltype(auto) {
    if constexpr (std::is_void_v<AwaiterResultType>) {
      return std::type_identity<decltype(std::declval<CallbackStorage&>()())>{};
    } else {
      return std::type_identity<decltype(std::declval<CallbackStorage&>()(std::declval<AwaiterResultType>()))>{};
    }
  })())::type;

  using CallbackReturn = typename CoroutineTraits<CallbackResultType>::CoroutineReturnTypeOrOriginalType;
  using SingleResultType = std::conditional_t<std::is_void_v<CallbackReturn>, bool, std::optional<CallbackReturn>>;
  using ResultType = std::vector<SingleResultType>;

  explicit SelectPairList() = default;

  void Add(Awaitable&& awaitable, Callback&& callback) {
    _pairs.push_back(Pair{std::forward<Awaitable>(awaitable), std::forward<Callback>(callback)});
  }

  struct ListAwaiter : public AwaiterBase<FiberSuspender> {
    mutable std::vector<std::unique_ptr<UnderlyingAwaiter>> _awaiters;

    explicit ListAwaiter(std::vector<Pair>& pairs) {
      _awaiters.reserve(pairs.size());
      for (auto& pair : pairs) {
        _awaiters.push_back(
            std::unique_ptr<UnderlyingAwaiter>(new UnderlyingAwaiter(std::move(pair.first).operator co_await())));
      }
    }

    bool await_ready() const {
      for (auto& awaiter : _awaiters) {
        if (awaiter->await_ready()) {
          return true;
        }
      }
      return false;
    }

    void OnAwaitSuspend() {
      auto& parent = this->GetOwnerPromise();
      for (auto& awaiter : _awaiters) {
        awaiter->SetOwnerPromise(parent);
        awaiter->OnAwaitSuspend();
      }
    }

    template <typename PromiseType> void await_suspend(std::coroutine_handle<PromiseType> caller) {
      this->DoAwaitSuspend(caller);
      OnAwaitSuspend();
    }

    std::vector<UnderlyingAwaiterResultOptionalType> await_resume() {
      std::vector<UnderlyingAwaiterResultOptionalType> results;
      results.reserve(_awaiters.size());
      for (auto& awaiter : _awaiters) {
        if (awaiter->await_ready()) {
          if constexpr (std::is_void_v<AwaiterResultType>) {
            awaiter->await_resume();
            results.push_back(true);
          } else {
            results.push_back(awaiter->await_resume());
          }
        } else {
          if constexpr (std::is_void_v<AwaiterResultType>) {
            results.push_back(false);
          } else {
            results.push_back(std::nullopt);
          }
        }
      }
      return results;
    }
  };

  ListAwaiter operator co_await() { return ListAwaiter(_pairs); }

  operator Awaiter() { return ListAwaiter(_pairs); }

  Coroutine<ResultType> RunCallback(AwaiterResultOptionalType& results) {
    std::vector<SingleResultType> final_results;
    final_results.reserve(_pairs.size());
    for (size_t i = 0; i < _pairs.size(); ++i) {
      UnderlyingAwaiterResultOptionalType res = results ? (*results)[i] : UnderlyingAwaiterResultOptionalType{};
      final_results.push_back(co_await RunPairCallback(_pairs[i], std::move(res)));
    }
    co_return final_results;
  }

private:
  static Coroutine<SingleResultType> RunPairCallback(Pair& pair, UnderlyingAwaiterResultOptionalType result) {
    auto& callback = pair.second;
    if constexpr (std::is_same_v<std::decay_t<decltype(result)>, bool>) {
      if (result) {
        if constexpr (std::is_same_v<SingleResultType, bool>) {
          if constexpr (requires { typename decltype(callback())::CoroutineReturnType; }) {
            co_await callback();
          } else {
            callback();
          }
          co_return true;
        } else {
          if constexpr (requires { typename decltype(callback())::CoroutineReturnType; }) {
            co_return SingleResultType(std::in_place, co_await callback());
          } else {
            co_return SingleResultType(std::in_place, callback());
          }
        }
      } else {
        if constexpr (std::is_same_v<SingleResultType, bool>) {
          co_return false;
        } else {
          co_return std::nullopt;
        }
      }
    } else {
      if (result.has_value()) {
        auto&& val = std::move(*result);
        if constexpr (std::is_same_v<SingleResultType, bool>) {
          if constexpr (requires { typename decltype(callback(std::move(val)))::CoroutineReturnType; }) {
            co_await callback(std::move(val));
          } else {
            callback(std::move(val));
          }
          co_return true;
        } else {
          if constexpr (requires { typename decltype(callback(std::move(val)))::CoroutineReturnType; }) {
            co_return SingleResultType(std::in_place, co_await callback(std::move(val)));
          } else {
            co_return SingleResultType(std::in_place, callback(std::move(val)));
          }
        }
      } else {
        if constexpr (std::is_same_v<SingleResultType, bool>) {
          co_return false;
        } else {
          co_return std::nullopt;
        }
      }
    }
  }

  std::vector<Pair> _pairs;
};

} // namespace Fiber
} // namespace Omni
