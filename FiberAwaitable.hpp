#pragma once

#include <coroutine>
#include <memory>
#include <optional>

#include "shared.h"

namespace Omni {
namespace Fiber {

class Fiber;
class FiberAwaitContext;

// Awaitables that can be co_awaited by fibers. It should be placed as a temporary object in co_await expression, and
// destroyed after the co_await expression is evaluated. Never hold it to an lvalue or a member variable.
class FiberAwaitable {
protected:
  OMNIFIBER_API explicit FiberAwaitable(std::shared_ptr<FiberAwaitContext> context);
  OMNIFIBER_API ~FiberAwaitable();

  FiberAwaitable(const FiberAwaitable&) = delete;
  FiberAwaitable& operator=(const FiberAwaitable&) = delete;
  FiberAwaitable(FiberAwaitable&&) = delete;
  FiberAwaitable& operator=(FiberAwaitable&&) = delete;

public:
  void Resume();

  template <typename PromiseType> void await_suspend(std::coroutine_handle<PromiseType> caller) {
    _Owner = caller.promise().GetFiber().shared_from_this();
    DoAwaitSuspend(caller);
  }

private:
  std::shared_ptr<FiberAwaitContext> _Context;
  std::optional<std::shared_ptr<Fiber>> _Owner;

  void DoAwaitSuspend(std::coroutine_handle<> caller);
};

} // namespace Fiber
} // namespace Omni
