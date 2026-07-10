#include "SharedAwaiter.hpp"

#include <memory>

#include "Fiber.hpp"
#include "SharedAwaitContext.hpp"

namespace Omni::Fiber {

auto SharedAwaiter::Get(ContextStorage& context) -> std::shared_ptr<SharedAwaitContext> {
  if (auto handle = context.lock()) {
    return handle;
  } else {
    auto newHandle = std::make_shared<SharedAwaitContext>();
    context = newHandle;
    return newHandle;
  }
}

void SharedAwaiter::Fire(ContextStorage& context) {
  if (auto handle = context.lock()) {
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

} // namespace Omni::Fiber
