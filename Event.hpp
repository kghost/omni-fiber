#pragma once

#include <memory>
#include <sys/epoll.h>

#include "FiberAwaitContext.hpp"
#include "FiberAwaitable.hpp"
#include "FiberAwaitableCustom.hpp"

#include "shared.h"

namespace Omni {
namespace Fiber {

class Event final {
public:
  explicit Event() = default;

  Event(Event&) = delete;
  Event& operator=(Event&) = delete;
  Event(Event&&) = delete;
  Event& operator=(Event&&) = delete;

  OMNIFIBER_API void Fire() {
    _Fired = true;
    if (auto awaitContext = _AwaitContext.lock()) {
      awaitContext->Fire();
    }
  }

  using AwaitResultType = void;
  bool AwaitReady() const { return _Fired; }
  void AwaitValue() {}
  OMNIFIBER_API FiberAwaitableCustom<Event> operator co_await() {
    return FiberAwaitableCustom<Event>{FiberAwaitContext::Get(_AwaitContext), *this};
  }

private:
  std::weak_ptr<FiberAwaitContext> _AwaitContext;
  bool _Fired = false;
};

} // namespace Fiber
} // namespace Omni