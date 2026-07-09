#pragma once

#include <coroutine>

namespace Omni::Fiber {

class Fiber;
class Manager;

class FiberPromise {
public:
  explicit FiberPromise() = default;
  virtual ~FiberPromise() = default;

  FiberPromise(FiberPromise&) = delete;
  auto operator=(const FiberPromise&) -> FiberPromise& = delete;
  FiberPromise(FiberPromise&&) = delete;
  auto operator=(FiberPromise&&) -> FiberPromise& = delete;

  virtual auto GetFiber() -> Fiber& = 0;
  virtual auto GetCoroutineHandle() noexcept -> std::coroutine_handle<> = 0;

#ifndef NDEBUG
  void SetInstructionPointer(void* instructionPointer) noexcept { _InstructionPointer = instructionPointer; }
  [[nodiscard]] auto GetInstructionPointer() const noexcept -> void* { return _InstructionPointer; }
  [[nodiscard]] virtual auto GetCallerPromise() const noexcept -> FiberPromise* { return nullptr; }

private:
  void* _InstructionPointer = nullptr;
#endif
};

} // namespace Omni::Fiber
