#include "FiberAwaitContext.hpp"
#include "FiberAwaitable.hpp"

namespace Omni {
namespace Fiber {

std::shared_ptr<FiberAwaitContext> FiberAwaitContext::Get(std::weak_ptr<FiberAwaitContext>& context) {
  if (auto sharedContext = context.lock()) {
    return sharedContext;
  } else {
    auto newContext = std::make_shared<FiberAwaitContext>();
    context = newContext;
    return newContext;
  }
}

void FiberAwaitContext::AddFiberAwaitable(FiberAwaitable& awaitable) {
  auto [it, inserted] = _PendingSet.emplace(awaitable, FiberAwaitableState::Waiting);
  assert(inserted && "Adding a FiberAwaitable that is already in the pending set");
}

void FiberAwaitContext::RemoveFiberAwaitable(FiberAwaitable& awaitable) {
  auto n = _PendingSet.erase(awaitable);
  assert(n == 1 && "Removing a FiberAwaitable that is not in the pending set");
}

void FiberAwaitContext::Fire() {
  for (auto& awaitable : _PendingSet) {
    if (awaitable.second == FiberAwaitableState::Waiting) {
      awaitable.second = FiberAwaitableState::Notified;
      awaitable.first.get().Resume();
    }
  }
}

} // namespace Fiber
} // namespace Omni
