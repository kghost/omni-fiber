#pragma once

#include <cstddef>
#include <tuple>
#include <type_traits>
#include <utility>

#include "AwaitableBase.hpp"
#include "Fiber.hpp"

namespace Omni {
namespace Fiber {

template <typename T> std::true_type IsAwaitableBaseHelper(const AwaitableBase<T>*);
std::false_type IsAwaitableBaseHelper(...);

template <typename T>
concept DerivedFromAwaitableBase = decltype(IsAwaitableBaseHelper(std::declval<std::decay_t<T>*>()))::value;

template <typename... Pairs> class SelectAwaiter : public AwaitableBase<SelectAwaiter<Pairs...>> {
private:
  template <typename Pair> using AwaiterType = decltype(std::declval<Pair>().first.operator co_await());
  template <typename Pair> using CallbackType = std::decay_t<decltype(std::declval<Pair>().second)>;

  static_assert((DerivedFromAwaitableBase<AwaiterType<Pairs>> && ...),
                "All awaiters in Select must derive from AwaitableBase");

  template <typename Pair> struct GetAwaiter {
    Pair& p;
    operator AwaiterType<Pair>() && { return std::get<0>(p).operator co_await(); }
  };

public:
  explicit SelectAwaiter(Pairs... pairs) : _Awaiters(GetAwaiter{pairs}...), _Callbacks(std::move(pairs.second)...) {}

  bool await_ready() const {
    return std::apply([](const auto&... awaiter) { return (awaiter.await_ready() || ...); }, _Awaiters);
  }

  void DoAwaitSuspend() {
    auto& parent = this->GetOwner();
    std::apply([&](auto&... awaiter) { ((awaiter.SetOwner(parent), awaiter.DoAwaitSuspend()), ...); }, _Awaiters);
  }

  void await_resume() {
    auto resume = [](auto& awaiter, auto& callback) -> void {
      if (awaiter.await_ready()) {
        if constexpr (std::is_void_v<decltype(awaiter.await_resume())>) {
          static_assert(std::is_void_v<std::invoke_result_t<decltype(callback)>>, "Callback must return void");
          awaiter.await_resume();
          callback();
        } else {
          static_assert(std::is_void_v<std::invoke_result_t<decltype(callback), decltype(awaiter.await_resume())>>,
                        "Callback must return void");
          callback(awaiter.await_resume());
        }
      }
    };
    [&]<size_t... Is>(std::index_sequence<Is...>) {
      (resume(std::get<Is>(_Awaiters), std::get<Is>(_Callbacks)), ...);
    }(std::index_sequence_for<Pairs...>{});
  }

private:
  std::tuple<AwaiterType<Pairs>...> _Awaiters;
  std::tuple<CallbackType<Pairs>...> _Callbacks;
};

template <typename Awaitable, typename Callback> auto SelectPair(Awaitable& awaitable, Callback&& callback) {
  return std::pair<Awaitable&, std::decay_t<Callback>>(awaitable, std::forward<Callback>(callback));
}

template <typename... Pairs> auto Select(Pairs&&... pairs) {
  return SelectAwaiter<Pairs...>(std::forward<Pairs>(pairs)...);
}

template <typename... Pairs> auto Select(std::tuple<Pairs...>&& pairsTuple) {
  return std::apply([](auto&&... args) { return Select(std::forward<decltype(args)>(args)...); },
                    std::move(pairsTuple));
}

} // namespace Fiber
} // namespace Omni
