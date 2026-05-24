#include "../common/header.h"

#include "Event.h"

#include "Fiber.h"
#include "Manager.h"

namespace Omni {
namespace Fiber {

Event::Awaitable Event::operator co_await() { return Event::Awaitable(*this); }

void Event::Set() {
  if (_IsSet)
    return;
  _IsSet = true;
  for (std::weak_ptr<Fiber> fiber : _PendingSet)
    fiber.lock()->Schedule();
  _PendingSet.clear();
}

void Event::Awaitable::await_suspend(std::coroutine_handle<> caller) {
  _Event._PendingSet.push_back(Manager::GetCurrentFiber());
  FiberAwaitable::await_suspend(caller);
}

} // namespace Fiber
} // namespace Omni
