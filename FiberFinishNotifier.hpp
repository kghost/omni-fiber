#pragma once

namespace Omni::Fiber {

class Fiber;

class FiberFinishNotifier {
public:
  explicit FiberFinishNotifier() = default;
  virtual ~FiberFinishNotifier() = default;

  FiberFinishNotifier(const FiberFinishNotifier&) = delete;
  auto operator=(const FiberFinishNotifier&) -> FiberFinishNotifier& = delete;
  FiberFinishNotifier(FiberFinishNotifier&&) = delete;
  auto operator=(FiberFinishNotifier&&) -> FiberFinishNotifier& = delete;

  virtual void OnFiberFinished(Fiber& fiber) = 0;
};

} // namespace Omni::Fiber
