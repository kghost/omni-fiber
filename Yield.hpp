#pragma once

#include "AwaiterBase.hpp"

namespace Omni::Fiber {

class Yield : public AwaiterBase<FiberYielder> {
public:
  explicit Yield() = default;

  constexpr bool await_ready() const noexcept { return false; }
  constexpr void await_resume() const noexcept {}
  template <typename PromiseType> void await_suspend(std::coroutine_handle<PromiseType> caller) {
    DoAwaitSuspend(caller);
  }
};

} // namespace Omni::Fiber
