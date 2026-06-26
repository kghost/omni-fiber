#pragma once

#include <functional>
#include <type_traits>
#include <utility>

#include "Coroutine.hpp"
#include "Event.hpp"
#include "MoveOnlyFunction.hpp"
#include "Pipe.hpp"

namespace Omni {
namespace Fiber {

class RemoteCall {
public:
  explicit RemoteCall() = default;
  ~RemoteCall() { assert(_Pipe.IsClosed()); }

  RemoteCall(const RemoteCall&) = delete;
  RemoteCall& operator=(const RemoteCall&) = delete;
  RemoteCall(RemoteCall&&) = delete;
  RemoteCall& operator=(RemoteCall&&) = delete;

  class CallFailed {};

  template <typename Func, typename Reply = decltype(std::declval<Func>()())::CoroutineReturnType>
  Coroutine<std::expected<Reply, CallFailed>> Call(Func&& func) {
    struct CallGuard {
      Event<std::expected<Reply, CallFailed>>& Ev;
      bool Fired = false;
      explicit CallGuard(Event<std::expected<Reply, CallFailed>>& ev) : Ev(ev) {}
      ~CallGuard() {
        if (!Fired) {
          Ev.Fire(std::unexpected<CallFailed>{CallFailed{}});
        }
      }
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

  Coroutine<void> Shutdown() { co_await _Pipe.GetProducer().Shutdown(); }
  void DiscardAndClose() { _Pipe.GetConsumer().DiscardAndClose(); }

  decltype(auto) GetServiceAwaitor() { return _Pipe.GetConsumer(); }

  Coroutine<bool> ProcessOne() { co_return co_await HandleRequest(co_await _Pipe.GetConsumer()); }

  Coroutine<void> Serve() {
    while (co_await ProcessOne()) {
      // Continue serving requests until closed
    }
  }

  static Coroutine<bool> HandleRequest(std::expected<move_only_function<Coroutine<void>()>, PipeClosed>&& req) {
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

} // namespace Fiber
} // namespace Omni
