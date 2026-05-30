#pragma once

#include <cassert>
#include <functional>
#include <optional>

#include "shared.h"

namespace Omni {
namespace Fiber {

class SingleAwaiter;

// Optimized context for events where at most one fiber is pending at a time.
// It avoids dynamic allocation or complex map/tree data structures.
class SingleAwaitContext final {
public:
  OMNIFIBER_API explicit SingleAwaitContext() = default;
  OMNIFIBER_API ~SingleAwaitContext() {
    assert(!_PendingAwaitable.has_value() && "SingleAwaitContext destroyed while there are still pending fibers");
  }

  SingleAwaitContext(SingleAwaitContext&) = delete;
  SingleAwaitContext& operator=(SingleAwaitContext&) = delete;
  SingleAwaitContext(SingleAwaitContext&&) = delete;
  SingleAwaitContext& operator=(SingleAwaitContext&&) = delete;

  OMNIFIBER_API void AddFiberAwaitable(SingleAwaiter& awaitable);
  OMNIFIBER_API void RemoveFiberAwaitable(SingleAwaiter& awaitable);
  OMNIFIBER_API void Fire();

private:
  enum class FiberAwaitableState {
    None,    // No fiber pending.
    Waiting, // Fiber pending.
    Notified // Fiber notified.
  };

  std::optional<std::reference_wrapper<SingleAwaiter>> _PendingAwaitable;
  FiberAwaitableState _State = FiberAwaitableState::None;
};

} // namespace Fiber
} // namespace Omni
