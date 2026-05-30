#include "SharedAwaitContext.hpp"

#include "SharedAwaiter.hpp"

namespace Omni {
namespace Fiber {

void SharedAwaitContext::AddFiberAwaitable(SharedAwaiter& awaitable) {
  auto [it, inserted] = _PendingSet.emplace(awaitable, FiberAwaitableState::Waiting);
  assert(inserted && "Adding a SharedAwaiter that is already in the pending set");
}

void SharedAwaitContext::RemoveFiberAwaitable(SharedAwaiter& awaitable) {
  auto n = _PendingSet.erase(awaitable);
  assert(n == 1 && "Removing a SharedAwaiter that is not in the pending set");
}

void SharedAwaitContext::Fire() {
  for (auto& awaitable : _PendingSet) {
    if (awaitable.second == FiberAwaitableState::Waiting) {
      awaitable.second = FiberAwaitableState::Notified;
      awaitable.first.get().Schedule();
    }
  }
}

} // namespace Fiber
} // namespace Omni
