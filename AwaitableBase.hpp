#pragma once

#include <coroutine>
#include <functional>
#include <optional>

#include "Fiber.hpp"

namespace Omni {
namespace Fiber {

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
  void SetOwner(Fiber& owner) { _Owner = owner; }
  Fiber& GetOwner() const { return _Owner.value().get(); }
  void Schedule(this Impl& self) { self._Owner.value().get().Schedule(); }

  template <typename PromiseType> void await_suspend(this Impl& self, std::coroutine_handle<PromiseType> caller) {
    auto& promise = caller.promise();
    self._Owner = promise.GetFiber();
#ifndef NDEBUG
    promise.SetInstructionPointer(__builtin_return_address(0));
    self._Owner.value().get().SetSuspendedPromise(&promise);
#endif
    self._Owner.value().get().Suspend(caller);
    self.DoAwaitSuspend();
  }

private:
  std::optional<std::reference_wrapper<Fiber>> _Owner;
};

} // namespace Fiber
} // namespace Omni
