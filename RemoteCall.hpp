#pragma once

#include <functional>
#include <type_traits>
#include <utility>

#include "Coroutine.hpp"
#include "Event.hpp"
#include "MoveOnlyFunction.hpp"
#include "Pipe.hpp"

namespace Omni::Fiber {

class RemoteCall {
public:
  explicit RemoteCall() = default;
  ~RemoteCall() { assert(_Pipe.IsClosed()); }

  RemoteCall(const RemoteCall&) = delete;
  auto operator=(const RemoteCall&) -> RemoteCall& = delete;
  RemoteCall(RemoteCall&&) = delete;
  auto operator=(RemoteCall&&) -> RemoteCall& = delete;

  class CallFailed {};

  template <typename Func, typename Reply = decltype(std::declval<Func>()())::CoroutineReturnType>
  Coroutine<std::expected<Reply, CallFailed>> Call(Func&& func) {
    struct CallGuard {
      explicit CallGuard(Event<std::expected<Reply, CallFailed>>& reply) : ReplyEvent(reply) {}
      ~CallGuard() {
        if (!Fired) {
          ReplyEvent.Fire(std::unexpected<CallFailed>{CallFailed{}});
        }
      }

      CallGuard(const CallGuard&) = delete;
      auto operator=(const CallGuard&) -> CallGuard& = delete;
      CallGuard(CallGuard&&) = delete;
      auto operator=(CallGuard&&) -> CallGuard& = delete;

      Event<std::expected<Reply, CallFailed>>& ReplyEvent;
      bool Fired = false;
    };

    Event<std::expected<Reply, CallFailed>> event;

    std::expected<void, PipeClosed> res =
        co_await _Pipe.GetProducer().Put([func = std::forward<Func>(func), &event,
                                          guard = std::make_shared<CallGuard>(event)]() mutable -> Coroutine<void> {
          if constexpr (std::is_void_v<Reply>) {
            co_await func();
            guard->Fired = true;
            event.Fire(std::expected<void, CallFailed>{});
          } else {
            auto val = co_await func();
            guard->Fired = true;
            event.Fire(std::expected<Reply, CallFailed>{std::move(val)});
          }
          co_return;
        });
    if (res.has_value()) {
      co_return co_await event;
    } else {
      co_return std::unexpected<CallFailed>{CallFailed{}};
    }
  }

  auto Shutdown() -> Coroutine<std::expected<void, CallFailed>> {
    co_return (co_await _Pipe.GetProducer().Shutdown()).transform_error([](PipeClosed) { return CallFailed{}; });
  }
  void DiscardAndClose() { _Pipe.GetConsumer().DiscardAndClose(); }

  auto GetServiceAwaitor() -> decltype(auto) { return _Pipe.GetConsumer(); }

  auto Serve() -> Coroutine<void> {
    while (co_await HandleRequest(co_await _Pipe.GetConsumer())) {
      // Continue serving requests until closed
    }
  }

  static auto HandleRequest(std::expected<move_only_function<Coroutine<void>()>, PipeClosed> req) -> Coroutine<bool> {
    if (req.has_value()) {
      co_await req.value()();
      co_return true;
    } else {
      co_return false;
    }
  }

private:
  Pipe<move_only_function<Coroutine<void>()>> _Pipe;
};

} // namespace Omni::Fiber
