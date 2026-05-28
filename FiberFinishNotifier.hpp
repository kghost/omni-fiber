#pragma once

namespace Omni {
namespace Fiber {

class Fiber;

class FiberFinishNotifier {
public:
  virtual ~FiberFinishNotifier() {}

  virtual void OnFiberFinished(Fiber& fiber) = 0;
};

} // namespace Fiber
} // namespace Omni
