#include "SingleAwaitContext.hpp"

#include "SingleAwaitable.hpp"

namespace Omni {
namespace Fiber {

void SingleAwaitContext::AddFiberAwaitable(SingleAwaitable& awaitable) {
  assert(!_PendingAwaitable.has_value() && _State == FiberAwaitableState::None &&
         "Adding a SingleAwaitable when one is already pending");
  _PendingAwaitable = awaitable;
  _State = FiberAwaitableState::Waiting;
}

void SingleAwaitContext::RemoveFiberAwaitable(SingleAwaitable& awaitable) {
  assert(_PendingAwaitable.has_value() && std::addressof(_PendingAwaitable->get()) == std::addressof(awaitable) &&
         "Removing a SingleAwaitable that is not the pending one");
  _PendingAwaitable = std::nullopt;
  _State = FiberAwaitableState::None;
}

void SingleAwaitContext::Fire() {
  if (_PendingAwaitable.has_value() && _State == FiberAwaitableState::Waiting) {
    _State = FiberAwaitableState::Notified;
    _PendingAwaitable->get().Resume();
  }
}

} // namespace Fiber
} // namespace Omni
