#pragma once

#include "FiberAwaitable.hpp"

#include "shared.h"

namespace Omni {
namespace Fiber {

class FiberAwaitContext;

template <typename Target> class FiberAwaitableCustom final : public FiberAwaitable {
public:
  OMNIFIBER_API explicit FiberAwaitableCustom(std::shared_ptr<FiberAwaitContext> context, Target& target)
      : FiberAwaitable(context), _Target(target) {}
  OMNIFIBER_API ~FiberAwaitableCustom() {}

  bool await_ready() const noexcept { return _Target.AwaitReady(); }
  typename Target::AwaitResultType await_resume() { return _Target.AwaitValue(); }

private:
  Target& _Target;
};

} // namespace Fiber
} // namespace Omni
