#pragma once

#include <optional>
#include <type_traits>
#include <utility>

#include "AwaiterBase.hpp"
#include "Coroutine.hpp"

namespace Omni {
namespace Fiber {

template <typename Awaitable, typename Callback> struct SelectPairImpl {
  using AwaitableType = Awaitable&&;
  using Awaiter = decltype(std::move(std::declval<AwaitableType>()).operator co_await());
  using AwaiterTraits = Omni::Fiber::AwaiterTraits<Awaiter>;
  using AwaiterResultType = typename AwaiterTraits::AwaiterResultType;
  using AwaiterResultOptionalType = typename AwaiterTraits::AwaiterResultOptionalType;

  using CallbackType = std::decay_t<Callback>;
  using CallbackResultType = decltype(([] -> decltype(auto) {
    if constexpr (std::is_void_v<AwaiterResultType>) {
      return std::type_identity<decltype(std::declval<CallbackType>()())>{};
    } else {
      return std::type_identity<decltype(std::declval<CallbackType>()(std::declval<AwaiterResultType>()))>{};
    }
  })())::type;

  using CallbackReturn = CoroutineTraits<CallbackResultType>::CoroutineReturnTypeOrOriginalType;
  using ResultType = std::conditional_t<std::is_void_v<CallbackReturn>, bool, std::optional<CallbackReturn>>;

  AwaitableType first;
  CallbackType second;

  operator Awaiter() { return std::move(first).operator co_await(); }

  template <typename Result> Coroutine<ResultType> RunCallback(Result& result) {
    auto& callback = this->second;
    if constexpr (std::is_same_v<std::decay_t<Result>, bool>) {
      if (result) {
        if constexpr (std::is_same_v<ResultType, bool>) {
          if constexpr (requires { typename decltype(callback())::CoroutineReturnType; }) {
            co_await callback();
          } else {
            callback();
          }
          co_return true;
        } else {
          if constexpr (requires { typename decltype(callback())::CoroutineReturnType; }) {
            co_return ResultType(std::in_place, co_await callback());
          } else {
            co_return ResultType(std::in_place, callback());
          }
        }
      } else {
        if constexpr (std::is_same_v<ResultType, bool>) {
          co_return false;
        } else {
          co_return std::nullopt;
        }
      }
    } else {
      if (result.has_value()) {
        auto&& val = std::move(*result);
        if constexpr (std::is_same_v<ResultType, bool>) {
          if constexpr (requires { typename decltype(callback(std::move(val)))::CoroutineReturnType; }) {
            co_await callback(std::move(val));
          } else {
            callback(std::move(val));
          }
          co_return true;
        } else {
          if constexpr (requires { typename decltype(callback(std::move(val)))::CoroutineReturnType; }) {
            co_return ResultType(std::in_place, co_await callback(std::move(val)));
          } else {
            co_return ResultType(std::in_place, callback(std::move(val)));
          }
        }
      } else {
        if constexpr (std::is_same_v<ResultType, bool>) {
          co_return false;
        } else {
          co_return std::nullopt;
        }
      }
    }
  }
};

template <typename Awaitable, typename Callback> auto SelectPair(Awaitable&& awaitable, Callback&& callback) {
  return SelectPairImpl<Awaitable, Callback>{std::forward<Awaitable>(awaitable), std::forward<Callback>(callback)};
}

} // namespace Fiber
} // namespace Omni
