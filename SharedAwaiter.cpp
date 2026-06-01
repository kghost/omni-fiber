#include "SharedAwaiter.hpp"

#include "Fiber.hpp"
#include "SharedAwaitContext.hpp"

namespace Omni {
namespace Fiber {

std::shared_ptr<SharedAwaitContext> SharedAwaiter::Get(ContextStorage& storage) {
  if (auto handle = storage.lock()) {
    return handle;
  } else {
    auto newHandle = std::make_shared<SharedAwaitContext>();
    storage = newHandle;
    return newHandle;
  }
}

void SharedAwaiter::Fire(ContextStorage& storage) {
  if (auto handle = storage.lock()) {
    handle->Fire();
  }
}

SharedAwaiter::SharedAwaiter(ContextStorage& context) : _Context(Get(context)) {}

SharedAwaiter::~SharedAwaiter() {
  if (this->IsSuspended()) {
    _Context->RemoveFiberAwaitable(*this);
  }
}

void SharedAwaiter::OnAwaitSuspend() { _Context->AddFiberAwaitable(*this); }

} // namespace Fiber
} // namespace Omni
