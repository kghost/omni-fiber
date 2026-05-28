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

#ifndef NDEBUG
public:
  void SetInstructionPointer(void* ip) noexcept { _InstructionPointer = ip; }
  void* GetInstructionPointer() const noexcept { return _InstructionPointer; }
  virtual FiberPromise* GetCallerPromise() const noexcept { return nullptr; }

private:
  void* _InstructionPointer = nullptr;
#endif
};

} // namespace Fiber
} // namespace Omni