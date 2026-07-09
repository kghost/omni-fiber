#pragma once

#include <cassert>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>

namespace Omni::Fiber {

class Fiber;
class SharedAwaiter;

// This is a one time use awaitable that can be co_awaited by multiple fibers. When the event is triggered, all waiting
// fibers will be resumed. It registers all waiting fibers in the _PendingSet, if a fiber stops waiting, it is removed
// from the _PendingSet by destroy SharedAwaiter to avoid being resumed when the event is triggered.
// All waiters hold and share the ownership of the SharedAwaitContext, so the SharedAwaitContext will be automatically
// destroyed when all waiters are done waiting. The source of the event should hold a weak reference to the
// SharedAwaitContext to trigger the event by calling Fire() method.
class SharedAwaitContext final {
public:
  explicit SharedAwaitContext() = default;
  ~SharedAwaitContext() {
    assert(_PendingSet.empty() && "SharedAwaitContext destroyed while there are still pending fibers");
  }

  SharedAwaitContext(SharedAwaitContext&) = delete;
  auto operator=(SharedAwaitContext&) -> SharedAwaitContext& = delete;
  SharedAwaitContext(SharedAwaitContext&&) = delete;
  auto operator=(SharedAwaitContext&&) -> SharedAwaitContext& = delete;

  void AddFiberAwaitable(SharedAwaiter& awaitable);
  void RemoveFiberAwaitable(SharedAwaiter& awaitable);
  void Fire();

private:
  struct AddressCompare {
    auto operator()(std::reference_wrapper<SharedAwaiter> lhs, std::reference_wrapper<SharedAwaiter> rhs) const
        -> bool {
      return std::less<const SharedAwaiter*>{}(std::addressof(lhs.get()), std::addressof(rhs.get()));
    }
  };

  enum class FiberAwaitableState : std::uint8_t {
    Waiting, // The fiber is waiting for the event to be triggered, and the awaitable is in the pending set.
    Notified // The event has been triggered, the fiber has been resumed, but the awaitable has not been destroyed
  };
  std::map<std::reference_wrapper<SharedAwaiter>, FiberAwaitableState, AddressCompare> _PendingSet;
};

} // namespace Omni::Fiber
