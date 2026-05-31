#pragma once

#include "AwaiterBase.hpp"

namespace Omni::Fiber {

class Yield : public AwaiterBase<Yield, FiberYielder> {
public:
  explicit Yield() = default;

  constexpr bool await_ready() const noexcept { return false; }
  constexpr void await_resume() const noexcept {}

  void DoAwaitSuspend() {}
};

} // namespace Omni::Fiber
