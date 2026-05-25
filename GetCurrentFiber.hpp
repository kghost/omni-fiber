#pragma once

#include <coroutine>

#include "Fiber.h"

namespace Omni::Fiber {

class GetCurrentFiber {
public:
  constexpr bool await_ready() const noexcept { return false; }

  template <typename PromiseType> bool await_suspend(std::coroutine_handle<PromiseType> caller) noexcept {
    _Fiber = &caller.promise().GetFiber();
    return false;
  }

  Fiber& await_resume() const noexcept { return *_Fiber; }

private:
  Fiber* _Fiber = nullptr;
};

} // namespace Omni::Fiber
