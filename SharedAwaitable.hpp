#pragma once

#include <memory>

#include "AwaitableBase.hpp"
#include "shared.h"

namespace Omni {
namespace Fiber {

class Fiber;
class SharedAwaitContext;

// Awaitables that can be co_awaited by fibers. It should be placed as a temporary object in co_await expression, and
// destroyed after the co_await expression is evaluated. Never hold it to an lvalue or a member variable.
class SharedAwaitable : public AwaitableBase<SharedAwaitable> {
public:
  using ContextStorage = std::weak_ptr<SharedAwaitContext>;

  static std::shared_ptr<SharedAwaitContext> Get(ContextStorage& context);
  static void Fire(ContextStorage& context);

protected:
  OMNIFIBER_API explicit SharedAwaitable(ContextStorage& context);
  OMNIFIBER_API ~SharedAwaitable();

  SharedAwaitable(const SharedAwaitable&) = delete;
  SharedAwaitable& operator=(const SharedAwaitable&) = delete;
  SharedAwaitable(SharedAwaitable&&) = delete;
  SharedAwaitable& operator=(SharedAwaitable&&) = delete;

public:
  void DoSchedule() {}
  void DoAwaitSuspend();

private:
  std::shared_ptr<SharedAwaitContext> _Context;
};

} // namespace Fiber
} // namespace Omni
