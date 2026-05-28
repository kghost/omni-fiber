#pragma once

#include "Fiber.hpp"

namespace Omni::Fiber {

#ifndef NDEBUG
class StackTrace {
public:
  constexpr bool await_ready() const noexcept { return false; }

  template <typename PromiseType> bool await_suspend(std::coroutine_handle<PromiseType> caller) noexcept {
    auto& promise = caller.promise();
    auto& fiber = promise.GetFiber();
    promise.SetInstructionPointer(__builtin_return_address(0));
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
  constexpr bool await_ready() const noexcept { return false; }

  template <typename PromiseType> bool await_suspend(std::coroutine_handle<PromiseType> caller) noexcept {
    auto& promise = caller.promise();
    auto& fiber = promise.GetFiber();
    auto& manager = fiber.GetManager();
    promise.SetInstructionPointer(__builtin_return_address(0));
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
