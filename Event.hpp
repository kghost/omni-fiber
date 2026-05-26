#pragma once

#include <sys/epoll.h>

#include "AwaitableCustom.hpp"
#include "SharedAwaitable.hpp"

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
    SharedAwaitable::Fire(_AwaitContext);
  }

  bool AwaitReady() const { return _Fired; }
  void AwaitValue() {}
  OMNIFIBER_API AwaitableCustom<Event, SharedAwaitable> operator co_await() {
    return AwaitableCustom<Event, SharedAwaitable>(_AwaitContext, *this);
  }

private:
  SharedAwaitable::ContextStorage _AwaitContext;
  bool _Fired = false;
};

} // namespace Fiber
} // namespace Omni