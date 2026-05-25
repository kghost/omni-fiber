#include "Event.h"

#include "Fiber.h"
#include "Manager.h"

namespace Omni {
namespace Fiber {

void Event::Set() {
  if (_IsSet) {
    return;
  }
  _IsSet = true;
  for (std::weak_ptr<Fiber> fiber : _PendingSet) {
    fiber.lock()->Schedule();
  }
  _PendingSet.clear();
}

void Event::await_suspend(std::coroutine_handle<> caller) {
  _PendingSet.push_back(Manager::GetCurrentFiber());
  FiberAwaitable::await_suspend(caller);
}

} // namespace Fiber
} // namespace Omni
