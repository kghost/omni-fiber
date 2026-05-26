#pragma once

#include <coroutine>
#include <memory>
#include <optional>

#include "shared.h"

namespace Omni {
namespace Fiber {

class Fiber;
class SharedAwaitContext;

// Awaitables that can be co_awaited by fibers. It should be placed as a temporary object in co_await expression, and
// destroyed after the co_await expression is evaluated. Never hold it to an lvalue or a member variable.
class SharedAwaitable {
public:
  using ContextType = SharedAwaitContext;
  using ContextStorage = std::weak_ptr<SharedAwaitContext>;
  using ContextHandle = std::shared_ptr<SharedAwaitContext>;

  static ContextHandle Get(ContextStorage& context);
  static void Fire(ContextStorage& context);

protected:
  OMNIFIBER_API explicit SharedAwaitable(ContextStorage& context);
  OMNIFIBER_API ~SharedAwaitable();

  SharedAwaitable(const SharedAwaitable&) = delete;
  SharedAwaitable& operator=(const SharedAwaitable&) = delete;
  SharedAwaitable(SharedAwaitable&&) = delete;
  SharedAwaitable& operator=(SharedAwaitable&&) = delete;

public:
  void Resume();

  template <typename PromiseType> void await_suspend(std::coroutine_handle<PromiseType> caller) {
    _Owner = caller.promise().GetFiber().shared_from_this();
    DoAwaitSuspend(caller);
  }

private:
  std::shared_ptr<SharedAwaitContext> _Context;
  std::optional<std::shared_ptr<Fiber>> _Owner;

  void DoAwaitSuspend(std::coroutine_handle<> caller);
};

} // namespace Fiber
} // namespace Omni
