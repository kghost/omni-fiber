#pragma once

#include <coroutine>
#include <memory>
#include <optional>

#include "Fiber.hpp"

namespace Omni {
namespace Fiber {

class SingleAwaitContext;

template <typename Impl> class AwaitableBase {
protected:
  explicit AwaitableBase() = default;
  ~AwaitableBase() = default;

  AwaitableBase(const AwaitableBase&) = delete;
  AwaitableBase& operator=(const AwaitableBase&) = delete;
  AwaitableBase(AwaitableBase&&) = delete;
  AwaitableBase& operator=(AwaitableBase&&) = delete;

public:
  bool IsSuspended() const noexcept { return _Owner.has_value(); }

  void Schedule(this Impl& self) {
    self.DoSchedule();
    self._Owner.value()->Schedule();
  }

  template <typename PromiseType> void await_suspend(this Impl& self, std::coroutine_handle<PromiseType> caller) {
    auto& promise = caller.promise();
    self._Owner = promise.GetFiber().shared_from_this();
#ifndef NDEBUG
    promise.SetInstructionPointer(__builtin_return_address(0));
    self._Owner.value()->SetSuspendedPromise(&promise);
#endif
    self._Owner.value()->Suspend(caller);
    self.DoAwaitSuspend();
  }

private:
  std::optional<std::shared_ptr<Fiber>> _Owner;
};

} // namespace Fiber
} // namespace Omni
