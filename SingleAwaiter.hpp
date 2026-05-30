#pragma once

#include "AwaiterBase.hpp"
#include "shared.h"

namespace Omni {
namespace Fiber {

class SingleAwaitContext;

// Optimized awaitable base class that only allows one fiber pending on it.
class SingleAwaiter : public AwaiterBase<SingleAwaiter> {
public:
  using ContextStorage = SingleAwaitContext;

  static SingleAwaitContext& Get(ContextStorage& context);
  static void Fire(ContextStorage& context);

protected:
  OMNIFIBER_API explicit SingleAwaiter(ContextStorage& context);
  OMNIFIBER_API ~SingleAwaiter();

  SingleAwaiter(const SingleAwaiter&) = delete;
  SingleAwaiter& operator=(const SingleAwaiter&) = delete;
  SingleAwaiter(SingleAwaiter&&) = delete;
  SingleAwaiter& operator=(SingleAwaiter&&) = delete;

public:
  void DoAwaitSuspend();

private:
  SingleAwaitContext& _Context;
};

} // namespace Fiber
} // namespace Omni
