#pragma once

#include <list>
#include <memory>

#include "FiberAwaitable.h"

#include "shared.h"

namespace Omni {
namespace Fiber {

class Fiber;

class Event : public FiberAwaitable {
public:
  explicit Event() = default;

  Event(Event&) = delete;
  Event& operator=(Event&) = delete;
  Event(Event&&) = delete;
  Event& operator=(Event&&) = delete;

  OMNIFIBER_API bool IsSet() { return _IsSet; }
  OMNIFIBER_API void Reset() { _IsSet = false; }

  OMNIFIBER_API void Set();

  OMNIFIBER_API bool await_ready() { return IsSet(); }

  template <typename PromiseType> void await_suspend(std::coroutine_handle<PromiseType> caller) {
    _PendingSet.push_back(caller.promise().GetFiber().shared_from_this());
    FiberAwaitable::await_suspend(caller);
  }

  OMNIFIBER_API Event& operator co_await() { return *this; }

private:
  bool _IsSet = false;
  std::list<std::weak_ptr<Fiber>> _PendingSet;
};

} // namespace Fiber
} // namespace Omni