#pragma once

#include <functional>
#include <type_traits>
#include <utility>

#include "Coroutine.hpp"
#include "OneshotEvent.hpp"
#include "SharedPipe.hpp"

namespace Omni {
namespace Fiber {

class RemoteCall {
public:
  explicit RemoteCall() = default;

  RemoteCall(const RemoteCall&) = delete;
  RemoteCall& operator=(const RemoteCall&) = delete;
  RemoteCall(RemoteCall&&) = delete;
  RemoteCall& operator=(RemoteCall&&) = delete;

  template <typename Func, typename Reply = decltype(std::declval<Func>()())::CoroutineReturnType>
  Coroutine<Reply> Call(Func func) {
    OneshotEvent<Reply> event;
    co_await _Pipe.GetProducer().Put([func = std::move(func), &event]() mutable -> Coroutine<void> {
      if constexpr (std::is_void_v<Reply>) {
        co_await func();
        event.Fire();
      } else {
        event.Fire(co_await func());
      }
      co_return;
    });
    co_return co_await event;
  }

  Coroutine<bool> ProcessOne() {
    auto req_opt = co_await _Pipe.GetConsumer();
    if (!req_opt.has_value()) {
      co_return false;
    }
    co_await req_opt.value()();
    co_return true;
  }

  Coroutine<void> Serve() {
    while (co_await ProcessOne()) {
      // Continue serving requests until closed
    }
  }

  auto GetServiceAwaitor() { return _Pipe.GetConsumer(); }

  Coroutine<void> Close() { co_await _Pipe.GetProducer().Close(); }

private:
  SharedPipe<std::move_only_function<Coroutine<void>()>> _Pipe;
};

} // namespace Fiber
} // namespace Omni
