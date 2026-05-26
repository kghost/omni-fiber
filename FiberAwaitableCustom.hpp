#pragma once

#include "shared.h"

namespace Omni {
namespace Fiber {

template <typename Target, typename BaseAwaitable> class FiberAwaitableCustom final : public BaseAwaitable {
public:
  using ContextType = typename BaseAwaitable::ContextType;

  OMNIFIBER_API explicit FiberAwaitableCustom(BaseAwaitable::ContextStorage& storage, Target& target)
      : BaseAwaitable(storage), _Target(target) {}
  OMNIFIBER_API ~FiberAwaitableCustom() {}

  bool await_ready() const noexcept { return _Target.AwaitReady(); }
  decltype(auto) await_resume() { return _Target.AwaitValue(); }

private:
  Target& _Target;
};

} // namespace Fiber
} // namespace Omni
