#pragma once

#include <cassert>
#include <functional>
#include <optional>

namespace Omni {
namespace Fiber {

class SingleAwaiter;

// Optimized context for events where at most one fiber is pending at a time.
// It avoids dynamic allocation or complex map/tree data structures.
class SingleAwaitContext final {
public:
  explicit SingleAwaitContext() = default;
  ~SingleAwaitContext() {
    assert(!_PendingAwaitable.has_value() && "SingleAwaitContext destroyed while there are still pending fibers");
  }

  SingleAwaitContext(SingleAwaitContext&) = delete;
  SingleAwaitContext& operator=(SingleAwaitContext&) = delete;
  SingleAwaitContext(SingleAwaitContext&&) = delete;
  SingleAwaitContext& operator=(SingleAwaitContext&&) = delete;

  void AddFiberAwaitable(SingleAwaiter& awaitable);
  void RemoveFiberAwaitable(SingleAwaiter& awaitable);
  void Fire();

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
