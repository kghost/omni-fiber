#pragma once

#include "shared.h"

namespace Omni {
namespace Fiber {

template <typename BaseAwaitable> class FiberAwaitableAlwaysSuspend final : public BaseAwaitable {
public:
  using ContextType = typename BaseAwaitable::ContextType;

  OMNIFIBER_API explicit FiberAwaitableAlwaysSuspend(BaseAwaitable::ContextStorage& storage) : BaseAwaitable(storage) {}
  OMNIFIBER_API ~FiberAwaitableAlwaysSuspend() {}

  bool await_ready() const noexcept { return false; }
  void await_resume() {}
};

} // namespace Fiber
} // namespace Omni
