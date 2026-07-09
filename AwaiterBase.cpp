#include "AwaiterBase.hpp"

#include <coroutine>

#include "Fiber.hpp"

namespace Omni::Fiber {

void FiberActionBase::Schedule(Fiber& fiber) { fiber.Schedule(); }

#ifndef NDEBUG
void FiberActionBase::SetSuspendedPromise(Fiber& fiber, FiberPromise& promise) { fiber.SetSuspendedPromise(&promise); }
#endif

void FiberSuspender::DoSuspend(Fiber& fiber, std::coroutine_handle<> caller) { fiber.Suspend(caller); }
void FiberYielder::DoSuspend(Fiber& fiber, std::coroutine_handle<> caller) { fiber.OmniYield(caller); }

} // namespace Omni::Fiber
