#pragma once

#include "AwaitableCustom.hpp"
#include "SharedAwaitable.hpp"

#include "shared.h"

namespace Omni {
namespace Fiber {

// One shot and forget notification
class Signal final {
public:
  explicit Signal() = default;

  Signal(Signal&) = delete;
  Signal& operator=(Signal&) = delete;
  Signal(Signal&&) = delete;
  Signal& operator=(Signal&&) = delete;

  OMNIFIBER_API void Fire() { SharedAwaitable::Fire(_AwaitContext); }
  bool AwaitReady() const { return false; }
  void AwaitValue() {}

  OMNIFIBER_API AwaitableCustom<Signal, SharedAwaitable> operator co_await() {
    return AwaitableCustom<Signal, SharedAwaitable>(_AwaitContext, *this);
  }

private:
  SharedAwaitable::ContextStorage _AwaitContext;
};

} // namespace Fiber
} // namespace Omni