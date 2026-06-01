#include "SingleAwaiter.hpp"

#include "AwaiterBase.hpp"
#include "Fiber.hpp"
#include "SingleAwaitContext.hpp"

namespace Omni {
namespace Fiber {

SingleAwaitContext& SingleAwaiter::Get(SingleAwaiter::ContextStorage& storage) { return storage; }

void SingleAwaiter::Fire(ContextStorage& storage) { storage.Fire(); }

SingleAwaiter::SingleAwaiter(ContextStorage& storage) : _Context(Get(storage)) {}

SingleAwaiter::~SingleAwaiter() {
  if (this->IsSuspended()) {
    _Context.RemoveFiberAwaitable(*this);
  }
}

void SingleAwaiter::OnAwaitSuspend() { _Context.AddFiberAwaitable(*this); }

} // namespace Fiber
} // namespace Omni
