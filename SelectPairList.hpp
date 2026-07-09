#pragma once

#include <expected>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

#include "AwaiterBase.hpp"
#include "Coroutine.hpp"

namespace Omni::Fiber {

template <typename Awaitable, typename Callback> class SelectPairList {
public:
  using AwaitableStorage = std::conditional_t<std::is_reference_v<Awaitable>, Awaitable, std::decay_t<Awaitable>>;
  using CallbackStorage = std::decay_t<Callback>;

  struct Pair {
    AwaitableStorage first;
    CallbackStorage second;
  };

  struct ListAwaiter;

  using UnderlyingAwaiter = decltype(std::move(std::declval<AwaitableStorage&>()).operator co_await());
  using AwaiterTraitsType = AwaiterTraits<UnderlyingAwaiter>;
  using AwaiterResultType = typename AwaiterTraitsType::AwaiterResultType;
  using UnderlyingAwaiterResultExpectedType = typename AwaiterTraitsType::AwaiterResultExpectedType;

  struct ListAwaiter : public AwaiterBase<FiberSuspender> {
    mutable std::vector<std::unique_ptr<UnderlyingAwaiter>> _awaiters;

    explicit ListAwaiter(std::vector<Pair>& pairs) {
      _awaiters.reserve(pairs.size());
      for (auto& pair : pairs) {
        _awaiters.push_back(
            std::unique_ptr<UnderlyingAwaiter>(new UnderlyingAwaiter(std::move(pair.first).operator co_await())));
      }
    }

    auto await_ready() const -> bool {
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

    auto await_resume() -> std::vector<UnderlyingAwaiterResultExpectedType> {
      std::vector<UnderlyingAwaiterResultExpectedType> results;
      results.reserve(_awaiters.size());
      for (auto& awaiter : _awaiters) {
        if (awaiter->await_ready()) {
          if constexpr (std::is_void_v<AwaiterResultType>) {
            awaiter->await_resume();
            results.push_back({});
          } else {
            results.push_back(awaiter->await_resume());
          }
        } else {
          results.push_back(std::unexpected(typename AwaiterTraitsType::AwaiterNotReady{}));
        }
      }
      return results;
    }
  };

  using Awaiter = ListAwaiter;
  using AwaiterResultExpectedType = typename AwaiterTraits<ListAwaiter>::AwaiterResultExpectedType;

  using CallbackResultType = decltype(([] -> decltype(auto) {
    if constexpr (std::is_void_v<AwaiterResultType>) {
      return std::type_identity<decltype(std::declval<CallbackStorage&>()())>{};
    } else {
      return std::type_identity<decltype(std::declval<CallbackStorage&>()(std::declval<AwaiterResultType>()))>{};
    }
  })())::type;

  using CallbackReturn = typename CoroutineTraits<CallbackResultType>::CoroutineReturnTypeOrOriginalType;
  using SingleResultType = std::expected<CallbackReturn, typename AwaiterTraitsType::AwaiterNotReady>;
  using ResultType = std::vector<SingleResultType>;

  explicit SelectPairList() = default;

  [[nodiscard]] auto Empty() const -> bool { return _pairs.empty(); }

  void Add(auto&& awaitable, auto&& callback) {
    _pairs.push_back(Pair{std::forward<decltype(awaitable)>(awaitable), std::forward<decltype(callback)>(callback)});
  }

  auto operator co_await() -> ListAwaiter { return ListAwaiter(_pairs); }

  operator Awaiter() { return ListAwaiter(_pairs); }

  auto RunCallback(AwaiterResultExpectedType& results) -> Coroutine<ResultType> {
    std::vector<SingleResultType> final_results;
    final_results.reserve(_pairs.size());
    for (size_t i = 0; i < _pairs.size(); ++i) {
      UnderlyingAwaiterResultExpectedType res =
          results.has_value() ? (*results)[i] : std::unexpected(typename AwaiterTraitsType::AwaiterNotReady{});
      final_results.push_back(co_await RunPairCallback(_pairs[i], std::move(res)));
    }
    co_return final_results;
  }

private:
  static auto RunPairCallback(Pair& pair, UnderlyingAwaiterResultExpectedType result) -> Coroutine<SingleResultType> {
    auto& callback = pair.second;
    if constexpr (std::is_void_v<AwaiterResultType>) {
      if (result.has_value()) {
        if constexpr (std::is_void_v<CallbackReturn>) {
          if constexpr (requires { typename decltype(callback())::CoroutineReturnType; }) {
            co_await callback();
          } else {
            callback();
          }
          co_return SingleResultType{};
        } else {
          if constexpr (requires { typename decltype(callback())::CoroutineReturnType; }) {
            co_return co_await callback();
          } else {
            co_return callback();
          }
        }
      } else {
        co_return std::unexpected(typename AwaiterTraitsType::AwaiterNotReady{});
      }
    } else {
      if (result.has_value()) {
        auto&& val = std::move(*result);
        if constexpr (std::is_void_v<CallbackReturn>) {
          if constexpr (requires { typename decltype(callback(std::move(val)))::CoroutineReturnType; }) {
            co_await callback(std::move(val));
          } else {
            callback(std::move(val));
          }
          co_return SingleResultType{};
        } else {
          if constexpr (requires { typename decltype(callback(std::move(val)))::CoroutineReturnType; }) {
            co_return co_await callback(std::move(val));
          } else {
            co_return callback(std::move(val));
          }
        }
      } else {
        co_return std::unexpected(typename AwaiterTraitsType::AwaiterNotReady{});
      }
    }
  }

  std::vector<Pair> _pairs;
};

} // namespace Omni::Fiber
