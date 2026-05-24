#include "../common/header.h"

#include "FiberAwaitable.h"

#include "Fiber.h"
#include "Manager.h"

namespace Omni {
namespace Fiber {

void FiberAwaitable::await_suspend(std::coroutine_handle<> caller) { Manager::GetCurrentFiber()->Suspend(caller); }

void FiberAwaitable::await_resume() {
  if (Manager::GetCurrentFiber()->_Interrupted)
    throw Fiber::FiberInterrupted();
}

} // namespace Fiber
} // namespace Omni