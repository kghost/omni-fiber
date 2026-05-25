#include "FiberAwaitable.h"

#include "Fiber.h"

namespace Omni {
namespace Fiber {

void FiberAwaitable::await_resume() {
  assert(_Fiber.has_value());
  if (_Fiber.value().get()._Interrupted) {
    throw Fiber::FiberInterrupted();
  }
}

} // namespace Fiber
} // namespace Omni