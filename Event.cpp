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

} // namespace Fiber
} // namespace Omni
