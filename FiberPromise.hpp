#pragma once

namespace Omni {
namespace Fiber {

class Fiber;
class Manager;

class FiberPromise {
public:
  explicit FiberPromise() = default;
  virtual ~FiberPromise() = default;

  FiberPromise(FiberPromise&) = delete;
  FiberPromise& operator=(const FiberPromise&) = delete;
  FiberPromise(FiberPromise&&) = delete;
  FiberPromise& operator=(FiberPromise&&) = delete;

  virtual Fiber& GetFiber() = 0;
  virtual Manager& GetManager() = 0;
};

} // namespace Fiber
} // namespace Omni