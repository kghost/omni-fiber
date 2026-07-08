#pragma once

#include <memory>

#include "AwaiterBase.hpp"
#include "SharedAwaitContext.hpp"

namespace Omni {
namespace Fiber {

// Awaitables that can be co_awaited by fibers. It should be placed as a temporary object in co_await expression, and
// destroyed after the co_await expression is evaluated. Never hold it to an lvalue or a member variable.
class SharedAwaiter : public AwaiterBase<FiberSuspender> {
public:
  using ContextStorage = std::weak_ptr<SharedAwaitContext>;

  static std::shared_ptr<SharedAwaitContext> Get(ContextStorage& context);
  static void Fire(ContextStorage& context);

protected:
  explicit SharedAwaiter(ContextStorage& context);
  ~SharedAwaiter();

  SharedAwaiter(const SharedAwaiter&) = delete;
  SharedAwaiter& operator=(const SharedAwaiter&) = delete;
  SharedAwaiter(SharedAwaiter&&) = delete;
  SharedAwaiter& operator=(SharedAwaiter&&) = delete;

public:
  template <typename PromiseType> void await_suspend(std::coroutine_handle<PromiseType> caller) {
    DoAwaitSuspend(caller);
    OnAwaitSuspend();
  }

  void OnAwaitSuspend();

private:
  std::shared_ptr<SharedAwaitContext> _Context;
};

} // namespace Fiber
} // namespace Omni
