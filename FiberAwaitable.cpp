#include "FiberAwaitable.hpp"

#include "Fiber.hpp"

namespace Omni {
namespace Fiber {

FiberAwaitable::FiberAwaitable(std::shared_ptr<FiberAwaitContext> context) : _Context(std::move(context)) {}

FiberAwaitable::~FiberAwaitable() {
  if (_Owner.has_value()) {
    _Context->RemoveFiberAwaitable(*this);
  }
}
void FiberAwaitable::Resume() { _Owner.value()->Schedule(); }

void FiberAwaitable::DoAwaitSuspend(std::coroutine_handle<> caller) {
  _Context->AddFiberAwaitable(*this);
  _Owner.value()->Suspend(caller);
}

} // namespace Fiber
} // namespace Omni
