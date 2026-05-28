#pragma once

#include "AwaitableBase.hpp"
#include "shared.h"

namespace Omni {
namespace Fiber {

class Fiber;
class SingleAwaitContext;

// Optimized awaitable base class that only allows one fiber pending on it.
class SingleAwaitable : public AwaitableBase<SingleAwaitable> {
public:
  using ContextStorage = SingleAwaitContext;

  static SingleAwaitContext& Get(ContextStorage& context);
  static void Fire(ContextStorage& context);

protected:
  OMNIFIBER_API explicit SingleAwaitable(ContextStorage& context);
  OMNIFIBER_API ~SingleAwaitable();

  SingleAwaitable(const SingleAwaitable&) = delete;
  SingleAwaitable& operator=(const SingleAwaitable&) = delete;
  SingleAwaitable(SingleAwaitable&&) = delete;
  SingleAwaitable& operator=(SingleAwaitable&&) = delete;

public:
  void DoSchedule();
  void DoAwaitSuspend();

private:
  SingleAwaitContext& _Context;
};

} // namespace Fiber
} // namespace Omni
