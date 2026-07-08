#pragma once

namespace Omni {
namespace Fiber {

template <typename BaseAwaitable> class AwaiterAlwaysSuspend final : public BaseAwaitable {
public:
  explicit AwaiterAlwaysSuspend(BaseAwaitable::ContextStorage& storage) : BaseAwaitable(storage) {}
  ~AwaiterAlwaysSuspend() {}

  bool await_ready() const noexcept { return false; }
  void await_resume() {}
};

} // namespace Fiber
} // namespace Omni
