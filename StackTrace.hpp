#pragma once

#include "Fiber.hpp"
#include "FiberPromise.hpp"
#include "Manager.hpp"

namespace Omni::Fiber {

#ifndef NDEBUG
class StackTrace {
public:
  [[nodiscard]] static constexpr auto await_ready() noexcept -> bool { return false; }

  template <typename PromiseType>
  auto await_suspend(std::coroutine_handle<PromiseType> caller,
                     // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast, performance-no-int-to-ptr)
                     void* instructionPointer = reinterpret_cast<void*>(
                         std::stacktrace::current(0, 1).at(0).native_handle())) noexcept -> bool {
    FiberPromise& promise = caller.promise();
    Fiber& fiber = promise.GetFiber();
    promise.SetInstructionPointer(instructionPointer);
    fiber.SetSuspendedPromise(&promise);
    boost::log::sources::severity_logger<boost::log::trivial::severity_level> logger;
    fiber.DumpCallStack(logger, 0);
    fiber.SetSuspendedPromise(nullptr);
    return false;
  }

  void await_resume() const noexcept {}

private:
};

class StackTraceAllFibers {
public:
  [[nodiscard]] static constexpr auto await_ready() noexcept -> bool { return false; }

  template <typename PromiseType>
  auto await_suspend(std::coroutine_handle<PromiseType> caller,
                     void* instructionPointer =
                         // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast, performance-no-int-to-ptr)
                     reinterpret_cast<void*>(std::stacktrace::current(0, 1).at(0).native_handle())) noexcept -> bool {
    FiberPromise& promise = caller.promise();
    Fiber& fiber = promise.GetFiber();
    Manager& manager = fiber.GetManager();
    promise.SetInstructionPointer(instructionPointer);
    fiber.SetSuspendedPromise(&promise);
    manager.DumpAllFibers();
    fiber.SetSuspendedPromise(nullptr);
    return false;
  }

  void await_resume() const noexcept {}

private:
};

#endif

} // namespace Omni::Fiber
