#pragma once

#include <list>
#include <memory>

#include "FiberAwaitable.h"

#include "shared.h"

namespace Omni {
namespace Fiber {

class Fiber;

class Event {
public:
  OMNIFIBER_API bool IsSet() { return _IsSet; }
  OMNIFIBER_API void Reset() { _IsSet = false; }

  OMNIFIBER_API void Set();

private:
  class Awaitable : public FiberAwaitable {
  public:
    OMNIFIBER_API Awaitable(Event& event) : _Event(event) {}

    OMNIFIBER_API bool await_ready() { return _Event.IsSet(); }
    OMNIFIBER_API void await_suspend(std::coroutine_handle<> caller);

  private:
    Event& _Event;
  };

public:
  OMNIFIBER_API Awaitable operator co_await();

private:
  bool _IsSet = false;
  std::list<std::weak_ptr<Fiber>> _PendingSet;
};

} // namespace Fiber
} // namespace Omni