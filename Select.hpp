#pragma once

#include <cstddef>
#include <optional>
#include <tuple>
#include <type_traits>
#include <utility>

#include "AwaitableBase.hpp"
#include "Coroutine.hpp"
#include "Fiber.hpp"

namespace Omni {
namespace Fiber {

template <typename Awaiter> struct AwaiterResult {
  using RawType = decltype(std::declval<Awaiter>().await_resume());
  using type = std::conditional_t<std::is_void_v<RawType>, bool, std::optional<RawType>>;
};

template <typename Awaiter> using AwaiterResultT = typename AwaiterResult<Awaiter>::type;

template <typename... Pairs> class SelectAwaiter : public AwaitableBase<SelectAwaiter<Pairs...>> {
private:
  template <typename Pair> using AwaiterType = decltype(std::declval<Pair>().first.operator co_await());

  static_assert(((requires { typename AwaiterType<Pairs>::AwaitableBaseImpl; }) && ...),
                "All awaiters in Select must derive from AwaitableBase");

  template <typename Pair> struct GetAwaiter {
    Pair& p;
    operator AwaiterType<Pair>() && { return std::get<0>(p).operator co_await(); }
  };

public:
  explicit SelectAwaiter(Pairs&... pairs) : _Awaiters(GetAwaiter<Pairs>{pairs}...) {}

  bool await_ready() const {
    return std::apply([](const auto&... awaiter) { return (awaiter.await_ready() || ...); }, _Awaiters);
  }

  void DoAwaitSuspend() {
    auto& parent = this->GetOwner();
    std::apply([&](auto&... awaiter) { ((awaiter.SetOwner(parent), awaiter.DoAwaitSuspend()), ...); }, _Awaiters);
  }

  std::tuple<AwaiterResultT<AwaiterType<Pairs>>...> await_resume() {
    auto resume = []<typename Awaiter>(Awaiter& awaiter) -> AwaiterResultT<Awaiter> {
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
  std::tuple<AwaiterType<Pairs>...> _Awaiters;
};

template <typename Awaitable, typename Callback> auto SelectPair(Awaitable&& awaitable, Callback&& callback) {
  return std::pair<Awaitable&&, std::decay_t<Callback>>(std::forward<Awaitable>(awaitable),
                                                        std::forward<Callback>(callback));
}

template <typename... Pairs> Coroutine<void> Select(Pairs... pairs) {
  auto results = co_await SelectAwaiter<Pairs...>(pairs...);

  auto pairs_tuple = std::forward_as_tuple(pairs...);

  co_await [&]<size_t... Is>(std::index_sequence<Is...>) -> Coroutine<void> {
    auto run_one = []<typename Pair, typename Result>(Pair& pair, Result& result) -> Coroutine<void> {
      auto& callback = pair.second;
      if constexpr (std::is_same_v<std::decay_t<Result>, bool>) {
        if (result) {
          if constexpr (requires { typename decltype(callback())::CoroutineReturnType; }) {
            co_await callback();
          } else {
            callback();
          }
        }
      } else {
        if (result.has_value()) {
          auto&& val = std::move(*result);
          if constexpr (requires { typename decltype(callback(std::move(val)))::CoroutineReturnType; }) {
            co_await callback(std::move(val));
          } else {
            callback(std::move(val));
          }
        }
      }
      co_return;
    };

    (co_await run_one(std::get<Is>(pairs_tuple), std::get<Is>(results)), ...);
  }(std::index_sequence_for<Pairs...>{});
}

} // namespace Fiber
} // namespace Omni
