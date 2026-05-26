#pragma once

#include <memory>

#include "FiberAwaitable.hpp"

#include "shared.h"

namespace Omni {
namespace Fiber {

class FiberAwaitContext;

class FiberAwaitableAlwaysSuspend final : public FiberAwaitable {
public:
  OMNIFIBER_API explicit FiberAwaitableAlwaysSuspend(std::shared_ptr<FiberAwaitContext> context)
      : FiberAwaitable(context) {}
  OMNIFIBER_API ~FiberAwaitableAlwaysSuspend() {}

  bool await_ready() const noexcept { return false; }
  void await_resume() {}
};

} // namespace Fiber
} // namespace Omni
