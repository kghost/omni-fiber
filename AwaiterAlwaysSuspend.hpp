#pragma once

namespace Omni::Fiber {

template <typename BaseAwaitable> class AwaiterAlwaysSuspend final : public BaseAwaitable {
public:
  explicit AwaiterAlwaysSuspend(BaseAwaitable::ContextStorage& storage) : BaseAwaitable(storage) {}
  ~AwaiterAlwaysSuspend() = default;

  AwaiterAlwaysSuspend(const AwaiterAlwaysSuspend&) = delete;
  auto operator=(const AwaiterAlwaysSuspend&) -> AwaiterAlwaysSuspend& = delete;
  AwaiterAlwaysSuspend(AwaiterAlwaysSuspend&&) = delete;
  auto operator=(AwaiterAlwaysSuspend&&) -> AwaiterAlwaysSuspend& = delete;

  [[nodiscard]] auto await_ready() const noexcept -> bool { return false; }
  void await_resume() {}
};

} // namespace Omni::Fiber
