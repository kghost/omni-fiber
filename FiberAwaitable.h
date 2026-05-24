#pragma once

#include <coroutine>

#include "shared.h"

namespace Omni {
namespace Fiber {

class Fiber;

// To suspend a fiber
class FiberAwaitable {
public:
  OMNIFIBER_API FiberAwaitable() {}

  OMNIFIBER_API bool await_ready() { return true; }
  OMNIFIBER_API void await_suspend(std::coroutine_handle<> caller);
  OMNIFIBER_API void await_resume();
};

} // namespace Fiber
} // namespace Omni