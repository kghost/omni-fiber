#pragma once

#include <cstddef>
#include <optional>
#include <tuple>
#include <type_traits>
#include <utility>

#include "AwaiterBase.hpp"
#include "Coroutine.hpp"

namespace Omni {
namespace Fiber {

template <typename Awaitable, typename Callback> struct SelectPairImpl {
  using AwaitableType = Awaitable&&;
  using Awaiter = decltype(std::declval<AwaitableType>().operator co_await());
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

  operator Awaiter() { return first.operator co_await(); }

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

template <typename... Pairs> class SelectAwaiter : public AwaiterBase<SelectAwaiter<Pairs...>> {
private:
  static_assert(((requires { typename Pairs::Awaiter::AwaiterBaseImpl; }) && ...),
                "All awaiters in Select must derive from AwaiterBase");

public:
  explicit SelectAwaiter(Pairs&... pairs) : _Awaiters(pairs...) {}

  bool await_ready() const {
    return std::apply([](const auto&... awaiter) { return (awaiter.await_ready() || ...); }, _Awaiters);
  }

  void DoAwaitSuspend() {
    auto& parent = this->GetOwner();
    std::apply([&](auto&... awaiter) { ((awaiter.SetOwner(parent), awaiter.DoAwaitSuspend()), ...); }, _Awaiters);
  }

  std::tuple<typename Pairs::AwaiterResultOptionalType...> await_resume() {
    auto resume = []<typename Awaiter>(Awaiter& awaiter) -> typename AwaiterTraits<Awaiter>::AwaiterResultOptionalType {
      if (awaiter.await_ready()) {
        if constexpr (std::is_void_v<decltype(awaiter.await_resume())>) {
          awaiter.await_resume();
          return true;
        } else {
          return awaiter.await_resume();
        }
      } else {
        if constexpr (std::is_void_v<decltype(awaiter.await_resume())>) {
          return false;
        } else {
          return std::nullopt;
        }
      }
    };

    return [&]<size_t... Is>(std::index_sequence<Is...>) {
      return std::make_tuple(resume(std::get<Is>(_Awaiters))...);
    }(std::index_sequence_for<Pairs...>{});
  }

private:
  std::tuple<typename Pairs::Awaiter...> _Awaiters;
};

template <typename Awaitable, typename Callback> auto SelectPair(Awaitable&& awaitable, Callback&& callback) {
  return SelectPairImpl<Awaitable, Callback>{std::forward<Awaitable>(awaitable), std::forward<Callback>(callback)};
}

template <typename... Pairs> Coroutine<std::tuple<typename Pairs::ResultType...>> Select(Pairs... pairs) {
  auto results = co_await SelectAwaiter<Pairs...>(pairs...);

  auto pairs_tuple = std::forward_as_tuple(pairs...);

  std::tuple<typename Pairs::ResultType...> final_results;

  co_await [&]<size_t... Is>(std::index_sequence<Is...>) -> Coroutine<void> {
    ((std::get<Is>(final_results) = co_await std::get<Is>(pairs_tuple).RunCallback(std::get<Is>(results))), ...);
  }(std::index_sequence_for<Pairs...>{});

  co_return final_results;
}

} // namespace Fiber
} // namespace Omni
