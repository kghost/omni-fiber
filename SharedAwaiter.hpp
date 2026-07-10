#pragma once

#include <memory>

#include "AwaiterBase.hpp"
#include "SharedAwaitContext.hpp"

namespace Omni::Fiber {

// Awaitables that can be co_awaited by fibers. It should be placed as a temporary object in co_await expression, and
// destroyed after the co_await expression is evaluated. Never hold it to an lvalue or a member variable.
class SharedAwaiter : public AwaiterBase<FiberSuspender> {
public:
  using ContextStorage = std::weak_ptr<SharedAwaitContext>;

  static auto Get(ContextStorage& context) -> std::shared_ptr<SharedAwaitContext>;
  static void Fire(ContextStorage& context);

protected:
  explicit SharedAwaiter(ContextStorage& context);
  ~SharedAwaiter();

public:
  SharedAwaiter(const SharedAwaiter&) = delete;
  auto operator=(const SharedAwaiter&) -> SharedAwaiter& = delete;
  SharedAwaiter(SharedAwaiter&&) = delete;
  auto operator=(SharedAwaiter&&) -> SharedAwaiter& = delete;

  template <typename PromiseType> void await_suspend(std::coroutine_handle<PromiseType> caller) {
    DoAwaitSuspend(caller);
    OnAwaitSuspend();
  }

  void OnAwaitSuspend();

private:
  std::shared_ptr<SharedAwaitContext> _Context;
};

} // namespace Omni::Fiber
