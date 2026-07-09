#pragma once

#include <expected>
#include <type_traits>
#include <utility>

#include "AwaiterBase.hpp"
#include "Coroutine.hpp"

namespace Omni::Fiber {

template <typename Awaitable, typename Callback> struct SelectPairImpl {
  using AwaitableType = Awaitable&&;
  using Awaiter = decltype(std::move(std::declval<AwaitableType>()).operator co_await());
  using AwaiterTraits = Omni::Fiber::AwaiterTraits<Awaiter>;
  using AwaiterResultType = typename AwaiterTraits::AwaiterResultType;
  using AwaiterResultExpectedType = typename AwaiterTraits::AwaiterResultExpectedType;

  using CallbackType = std::decay_t<Callback>;
  using CallbackResultType = decltype(([] -> decltype(auto) {
    if constexpr (std::is_void_v<AwaiterResultType>) {
      return std::type_identity<decltype(std::declval<CallbackType>()())>{};
    } else {
      return std::type_identity<decltype(std::declval<CallbackType>()(std::declval<AwaiterResultType>()))>{};
    }
  })())::type;

  using CallbackReturn = typename CoroutineTraits<CallbackResultType>::CoroutineReturnTypeOrOriginalType;
  using ResultType = std::expected<CallbackReturn, typename AwaiterTraits::AwaiterNotReady>;

  AwaitableType first;
  CallbackType second;

  operator Awaiter() { return std::move(first).operator co_await(); }

  auto RunCallback(AwaiterResultExpectedType& result) -> Coroutine<ResultType> {
    auto& callback = this->second;
    if constexpr (std::is_void_v<AwaiterResultType>) {
      if (result.has_value()) {
        if constexpr (std::is_void_v<CallbackReturn>) {
          if constexpr (requires { typename decltype(callback())::CoroutineReturnType; }) {
            co_await callback();
          } else {
            callback();
          }
          co_return ResultType{};
        } else {
          if constexpr (requires { typename decltype(callback())::CoroutineReturnType; }) {
            co_return co_await callback();
          } else {
            co_return callback();
          }
        }
      } else {
        co_return std::unexpected(typename AwaiterTraits::AwaiterNotReady{});
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
          co_return ResultType{};
        } else {
          if constexpr (requires { typename decltype(callback(std::move(val)))::CoroutineReturnType; }) {
            co_return co_await callback(std::move(val));
          } else {
            co_return callback(std::move(val));
          }
        }
      } else {
        co_return std::unexpected(typename AwaiterTraits::AwaiterNotReady{});
      }
    }
  }
};

template <typename Awaitable, typename Callback> auto SelectPair(Awaitable&& awaitable, Callback&& callback) {
  return SelectPairImpl<Awaitable, Callback>{std::forward<Awaitable>(awaitable), std::forward<Callback>(callback)};
}

} // namespace Omni::Fiber
