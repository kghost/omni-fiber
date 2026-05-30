#include "SingleAwaitContext.hpp"

#include "SingleAwaiter.hpp"

namespace Omni {
namespace Fiber {

void SingleAwaitContext::AddFiberAwaitable(SingleAwaiter& awaitable) {
  assert(!_PendingAwaitable.has_value() && _State == FiberAwaitableState::None &&
         "Adding a SingleAwaiter when one is already pending");
  _PendingAwaitable = awaitable;
  _State = FiberAwaitableState::Waiting;
}

void SingleAwaitContext::RemoveFiberAwaitable(SingleAwaiter& awaitable) {
  assert(_PendingAwaitable.has_value() && std::addressof(_PendingAwaitable->get()) == std::addressof(awaitable) &&
         "Removing a SingleAwaiter that is not the pending one");
  _PendingAwaitable = std::nullopt;
  _State = FiberAwaitableState::None;
}

void SingleAwaitContext::Fire() {
  if (_PendingAwaitable.has_value() && _State == FiberAwaitableState::Waiting) {
    _State = FiberAwaitableState::Notified;
    _PendingAwaitable->get().Schedule();
  }
}

} // namespace Fiber
} // namespace Omni
