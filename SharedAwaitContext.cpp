#include "SharedAwaitContext.hpp"

#include "SharedAwaitable.hpp"

namespace Omni {
namespace Fiber {

void SharedAwaitContext::AddFiberAwaitable(SharedAwaitable& awaitable) {
  auto [it, inserted] = _PendingSet.emplace(awaitable, FiberAwaitableState::Waiting);
  assert(inserted && "Adding a SharedAwaitable that is already in the pending set");
}

void SharedAwaitContext::RemoveFiberAwaitable(SharedAwaitable& awaitable) {
  auto n = _PendingSet.erase(awaitable);
  assert(n == 1 && "Removing a SharedAwaitable that is not in the pending set");
}

void SharedAwaitContext::Fire() {
  for (auto& awaitable : _PendingSet) {
    if (awaitable.second == FiberAwaitableState::Waiting) {
      awaitable.second = FiberAwaitableState::Notified;
      awaitable.first.get().Resume();
    }
  }
}

} // namespace Fiber
} // namespace Omni
