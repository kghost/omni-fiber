#pragma once

#include <memory>

namespace Omni {
namespace Fiber {

class Fiber;

class FiberFinishNotifier {
public:
  virtual ~FiberFinishNotifier() = 0 {}

  virtual void OnFiberFinished(std::shared_ptr<Fiber> fiber) = 0;
};

} // namespace Fiber
} // namespace Omni
