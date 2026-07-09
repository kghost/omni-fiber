#pragma once

#include "AwaiterBase.hpp"

namespace Omni::Fiber {

class OmniYield : public AwaiterBase<FiberYielder> {
public:
  explicit OmniYield() = default;

  [[nodiscard]] static constexpr auto await_ready() noexcept -> bool { return false; }
  constexpr void await_resume() const noexcept {}
  template <typename PromiseType> void await_suspend(std::coroutine_handle<PromiseType> caller) {
    DoAwaitSuspend(caller);
  }
};

} // namespace Omni::Fiber
