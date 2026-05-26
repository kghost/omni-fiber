#pragma once

#include <coroutine>
#include <memory>
#include <optional>

#include "shared.h"

namespace Omni {
namespace Fiber {

class Fiber;
class SingleAwaitContext;

// Optimized awaitable base class that only allows one fiber pending on it.
class SingleAwaitable {
public:
  using ContextType = SingleAwaitContext;
  using ContextStorage = SingleAwaitContext;
  using ContextHandle = SingleAwaitContext&;

  static ContextHandle Get(ContextStorage& context);
  static void Fire(ContextStorage& context);

protected:
  OMNIFIBER_API explicit SingleAwaitable(ContextStorage& context);
  OMNIFIBER_API ~SingleAwaitable();

  SingleAwaitable(const SingleAwaitable&) = delete;
  SingleAwaitable& operator=(const SingleAwaitable&) = delete;
  SingleAwaitable(SingleAwaitable&&) = delete;
  SingleAwaitable& operator=(SingleAwaitable&&) = delete;

public:
  void Resume();

  template <typename PromiseType> void await_suspend(std::coroutine_handle<PromiseType> caller) {
    _Owner = caller.promise().GetFiber().shared_from_this();
    DoAwaitSuspend(caller);
  }

private:
  ContextHandle _Context;
  std::optional<std::shared_ptr<Fiber>> _Owner;

  void DoAwaitSuspend(std::coroutine_handle<> caller);
};

} // namespace Fiber
} // namespace Omni
