#include "SharedAwaitable.hpp"

#include "Fiber.hpp"
#include "SharedAwaitContext.hpp"

namespace Omni {
namespace Fiber {

std::shared_ptr<SharedAwaitContext> SharedAwaitable::Get(ContextStorage& storage) {
  if (auto handle = storage.lock()) {
    return handle;
  } else {
    auto newHandle = std::make_shared<SharedAwaitContext>();
    storage = newHandle;
    return newHandle;
  }
}

void SharedAwaitable::Fire(ContextStorage& storage) {
  if (auto handle = storage.lock()) {
    handle->Fire();
  }
}

SharedAwaitable::SharedAwaitable(ContextStorage& context) : _Context(Get(context)) {}

SharedAwaitable::~SharedAwaitable() {
  if (_Owner.has_value()) {
    _Context->RemoveFiberAwaitable(*this);
  }
}
void SharedAwaitable::Schedule() { _Owner.value()->Schedule(); }

void SharedAwaitable::DoAwaitSuspend(std::coroutine_handle<> caller) {
  _Context->AddFiberAwaitable(*this);
  _Owner.value()->Suspend(caller);
}

} // namespace Fiber
} // namespace Omni
