#pragma once

#include "shared.h"

namespace Omni {
namespace Fiber {

template <typename BaseAwaitable> class AwaiterAlwaysSuspend final : public BaseAwaitable {
public:
  OMNIFIBER_API explicit AwaiterAlwaysSuspend(BaseAwaitable::ContextStorage& storage) : BaseAwaitable(storage) {}
  OMNIFIBER_API ~AwaiterAlwaysSuspend() {}

  bool await_ready() const noexcept { return false; }
  void await_resume() {}
};

} // namespace Fiber
} // namespace Omni
