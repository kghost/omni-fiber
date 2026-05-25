#pragma once

#include <coroutine>
#include <functional>
#include <optional>

#include "shared.h"

namespace Omni {
namespace Fiber {

class Fiber;

// To suspend a fiber
class FiberAwaitable {
public:
  OMNIFIBER_API FiberAwaitable() {}

  OMNIFIBER_API bool await_ready() { return true; }

  template <typename PromiseType> void await_suspend(std::coroutine_handle<PromiseType> caller) {
    auto& fiber = caller.promise().GetFiber();
    _Fiber.emplace(fiber);
    fiber.Suspend(caller);
  }

  OMNIFIBER_API void await_resume();

protected:
  std::optional<std::reference_wrapper<Fiber>> _Fiber;
};

} // namespace Fiber
} // namespace Omni
