#pragma once

#include <cstddef>
#include <expected>
#include <tuple>
#include <type_traits>
#include <utility>

#include "AwaiterBase.hpp"
#include "Coroutine.hpp"
#include "Tuple.hpp"

namespace Omni::Fiber {

template <typename... Awaitables> class SelectAwaiter : public AwaiterBase<FiberSuspender> {
private:
  static_assert(((requires { typename Awaitables::Awaiter::AwaiterBaseImpl; }) && ...),
                "All awaiters in Select must derive from AwaiterBase");
  static_assert(((requires { typename Awaitables::AwaiterResultExpectedType; }) && ...),
                "All awaiters in Select must have AwaiterResultExpectedType");

public:
  explicit SelectAwaiter(Awaitables&... aAwaitables)
      : _Awaiters{{static_cast<typename Awaitables::Awaiter>(aAwaitables)}...} {}

  [[nodiscard]] auto await_ready() const -> bool {
    return Apply([](const auto&... awaiter) -> auto { return (awaiter.await_ready() || ...); }, _Awaiters);
  }

  void OnAwaitSuspend() {
    auto& parent = this->GetOwnerPromise();
    Apply([&](auto&... awaiter) -> auto { ((awaiter.SetOwnerPromise(parent), awaiter.OnAwaitSuspend()), ...); },
          _Awaiters);
  }

  template <typename PromiseType> void await_suspend(std::coroutine_handle<PromiseType> caller) {
    DoAwaitSuspend(caller);
    OnAwaitSuspend();
  }

  auto await_resume() -> std::tuple<typename Awaitables::AwaiterResultExpectedType...> {
    auto resume = []<typename Awaiter>(Awaiter& awaiter) -> typename AwaiterTraits<Awaiter>::AwaiterResultExpectedType {
      if (awaiter.await_ready()) {
        if constexpr (std::is_void_v<decltype(awaiter.await_resume())>) {
          awaiter.await_resume();
          return {};
        } else {
          return awaiter.await_resume();
        }
      } else {
        return std::unexpected(typename AwaiterTraits<Awaiter>::AwaiterNotReady{});
      }
    };

    return [&]<size_t... Is>(std::index_sequence<Is...>) -> auto {
      return std::make_tuple(resume(Get<Is>(_Awaiters))...);
    }(std::index_sequence_for<Awaitables...>{});
  }

private:
  Tuple<typename Awaitables::Awaiter...> _Awaiters;
};

template <typename... Pairs> auto Select(Pairs... pairs) -> Coroutine<std::tuple<typename Pairs::ResultType...>> {
  auto results = co_await SelectAwaiter<Pairs...>(pairs...);

  auto pairs_tuple = std::forward_as_tuple(pairs...);

  std::tuple<typename Pairs::ResultType...> final_results;

  co_await [&]<size_t... Is>(std::index_sequence<Is...>) -> Coroutine<void> {
    ((std::get<Is>(final_results) = co_await std::get<Is>(pairs_tuple).RunCallback(std::get<Is>(results))), ...);
  }(std::index_sequence_for<Pairs...>{});

  co_return final_results;
}

} // namespace Omni::Fiber
