#pragma once

#include "shared.h"

namespace Omni {
namespace Fiber {

template <typename BaseAwaitable> class AwaitableAlwaysSuspend final : public BaseAwaitable {
public:
  OMNIFIBER_API explicit AwaitableAlwaysSuspend(BaseAwaitable::ContextStorage& storage) : BaseAwaitable(storage) {}
  OMNIFIBER_API ~AwaitableAlwaysSuspend() {}

  bool await_ready() const noexcept { return false; }
  void await_resume() {}
};

} // namespace Fiber
} // namespace Omni
