#include "SingleAwaitable.hpp"

#include "Fiber.hpp"
#include "SingleAwaitContext.hpp"

namespace Omni {
namespace Fiber {

SingleAwaitable::ContextHandle SingleAwaitable::Get(SingleAwaitable::ContextStorage& storage) { return storage; }

void SingleAwaitable::Fire(ContextStorage& storage) { storage.Fire(); }

SingleAwaitable::SingleAwaitable(ContextStorage& storage) : _Context(Get(storage)) {}

SingleAwaitable::~SingleAwaitable() {
  if (_Owner.has_value()) {
    _Context.RemoveFiberAwaitable(*this);
  }
}

void SingleAwaitable::Resume() { _Owner.value()->Schedule(); }

void SingleAwaitable::DoAwaitSuspend(std::coroutine_handle<> caller) {
  _Context.AddFiberAwaitable(*this);
  _Owner.value()->Suspend(caller);
}

} // namespace Fiber
} // namespace Omni
