#pragma once

#include "shared.h"

namespace Omni {
namespace Fiber {

template <typename Target, typename BaseAwaitable> class AwaitableCustom final : public BaseAwaitable {
public:
  OMNIFIBER_API explicit AwaitableCustom(BaseAwaitable::ContextStorage& storage, Target& target)
      : BaseAwaitable(storage), _Target(target) {}
  OMNIFIBER_API ~AwaitableCustom() {}

  bool await_ready() const noexcept { return _Target.AwaitReady(); }
  decltype(auto) await_resume() { return _Target.AwaitValue(); }

private:
  Target& _Target;
};

} // namespace Fiber
} // namespace Omni
