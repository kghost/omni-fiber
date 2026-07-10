#pragma once

namespace Omni::Fiber {

template <typename Target, typename BaseAwaitable> class AwaiterCustom final : public BaseAwaitable {
public:
  explicit AwaiterCustom(BaseAwaitable::ContextStorage& storage, Target& target)
      : BaseAwaitable(storage), _Target(target) {}
  ~AwaiterCustom() = default;

  AwaiterCustom(const AwaiterCustom&) = delete;
  auto operator=(const AwaiterCustom&) -> AwaiterCustom& = delete;
  AwaiterCustom(AwaiterCustom&&) = delete;
  auto operator=(AwaiterCustom&&) -> AwaiterCustom& = delete;

  [[nodiscard]] auto await_ready() const noexcept -> bool { return _Target.AwaitReady(); }
  auto await_resume() -> decltype(auto) { return _Target.AwaitValue(); }

private:
  Target& _Target;
};

} // namespace Omni::Fiber
