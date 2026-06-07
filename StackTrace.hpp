#pragma once

#include "Fiber.hpp"
#include "FiberPromise.hpp"
#include "Manager.hpp"

namespace Omni::Fiber {

#ifndef NDEBUG
class StackTrace {
public:
  constexpr bool await_ready() const noexcept { return false; }

  template <typename PromiseType>
  bool await_suspend(std::coroutine_handle<PromiseType> caller
#ifndef NDEBUG
                     ,
                     void* ip = (void*)std::stacktrace::current().at(0).native_handle()
#endif
                         ) noexcept {
    FiberPromise& promise = caller.promise();
    Fiber& fiber = promise.GetFiber();
    promise.SetInstructionPointer(ip);
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

  template <typename PromiseType>
  bool await_suspend(std::coroutine_handle<PromiseType> caller
#ifndef NDEBUG
                     ,
                     void* ip = (void*)std::stacktrace::current().at(0).native_handle()
#endif
                         ) noexcept {
    FiberPromise& promise = caller.promise();
    Fiber& fiber = promise.GetFiber();
    Manager& manager = fiber.GetManager();
    promise.SetInstructionPointer(ip);
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
