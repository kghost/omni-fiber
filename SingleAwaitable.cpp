#include "SingleAwaitable.hpp"

#include "Fiber.hpp"
#include "SingleAwaitContext.hpp"

namespace Omni {
namespace Fiber {

SingleAwaitContext& SingleAwaitable::Get(SingleAwaitable::ContextStorage& storage) { return storage; }

void SingleAwaitable::Fire(ContextStorage& storage) { storage.Fire(); }

SingleAwaitable::SingleAwaitable(ContextStorage& storage) : _Context(Get(storage)) {}

SingleAwaitable::~SingleAwaitable() {}

void SingleAwaitable::DoSchedule() {
  // _Context may already released when resumed, so we should avoid using _Context after Schedule()
  _Context.RemoveFiberAwaitable(*this);
}

void SingleAwaitable::DoAwaitSuspend() { _Context.AddFiberAwaitable(*this); }

} // namespace Fiber
} // namespace Omni
