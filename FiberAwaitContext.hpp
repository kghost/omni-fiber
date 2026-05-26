#pragma once

#include <cassert>
#include <functional>
#include <map>
#include <memory>

#include "shared.h"

namespace Omni {
namespace Fiber {

class Fiber;
class FiberAwaitable;

// This is a one time use awaitable that can be co_awaited by multiple fibers. When the event is triggered, all waiting
// fibers will be resumed. It registers all waiting fibers in the _PendingSet, if a fiber stops waiting, it is removed
// from the _PendingSet by destroy FiberAwaitable to avoid being resumed when the event is triggered.
// All waiters hold and share the ownership of the FiberAwaitContext, so the FiberAwaitContext will be automatically
// destroyed when all waiters are done waiting. The source of the event should hold a weak reference to the
// FiberAwaitContext to trigger the event by calling Fire() method.
class FiberAwaitContext final {
public:
  OMNIFIBER_API FiberAwaitContext() {}
  OMNIFIBER_API ~FiberAwaitContext() {
    assert(_PendingSet.empty() && "FiberAwaitContext destroyed while there are still pending fibers");
  }

  static std::shared_ptr<FiberAwaitContext> Get(std::weak_ptr<FiberAwaitContext>& context);

  FiberAwaitContext(FiberAwaitContext&) = delete;
  FiberAwaitContext& operator=(FiberAwaitContext&) = delete;
  FiberAwaitContext(FiberAwaitContext&&) = delete;
  FiberAwaitContext& operator=(FiberAwaitContext&&) = delete;

  OMNIFIBER_API void AddFiberAwaitable(FiberAwaitable& awaitable);
  OMNIFIBER_API void RemoveFiberAwaitable(FiberAwaitable& awaitable);
  OMNIFIBER_API void Fire();

private:
  struct AddressCompare {
    bool operator()(std::reference_wrapper<FiberAwaitable> lhs, std::reference_wrapper<FiberAwaitable> rhs) const {
      return std::less<const FiberAwaitable*>{}(std::addressof(lhs.get()), std::addressof(rhs.get()));
    }
  };

  enum class FiberAwaitableState {
    Waiting, // The fiber is waiting for the event to be triggered, and the awaitable is in the pending set.
    Notified // The event has been triggered, the fiber has been resumed, but the awaitable has not been destroyed
  };
  std::map<std::reference_wrapper<FiberAwaitable>, FiberAwaitableState, AddressCompare> _PendingSet;
};

} // namespace Fiber
} // namespace Omni
