#include "SharedAwaitContext.hpp"

#include "SharedAwaiter.hpp"

namespace Omni::Fiber {

void SharedAwaitContext::AddFiberAwaitable(SharedAwaiter& awaitable) {
  auto [iterator, inserted] = _PendingSet.emplace(awaitable, FiberAwaitableState::Waiting);
  assert(inserted && "Adding a SharedAwaiter that is already in the pending set");
}

void SharedAwaitContext::RemoveFiberAwaitable(SharedAwaiter& awaitable) {
  auto erased = _PendingSet.erase(awaitable);
  assert(erased == 1 && "Removing a SharedAwaiter that is not in the pending set");
}

void SharedAwaitContext::Fire() {
  for (auto& awaitable : _PendingSet) {
    if (awaitable.second == FiberAwaitableState::Waiting) {
      awaitable.second = FiberAwaitableState::Notified;
      awaitable.first.get().Schedule();
    }
  }
}

} // namespace Omni::Fiber
