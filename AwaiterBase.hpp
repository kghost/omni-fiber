#pragma once

#include <coroutine>
#include <functional>
#include <optional>

#include "Fiber.hpp"

namespace Omni {
namespace Fiber {

struct FiberSuspender {
  static void DoSuspend(Fiber& fiber, std::coroutine_handle<> caller) { fiber.Suspend(caller); }
};

struct FiberYielder {
  static void DoSuspend(Fiber& fiber, std::coroutine_handle<> caller) { fiber.Yield(caller); }
};

template <typename Awaiter> struct AwaiterTraits {
  using AwaiterResultType = decltype(std::declval<Awaiter>().await_resume());
  using AwaiterResultOptionalType =
      std::conditional_t<std::is_void_v<AwaiterResultType>, bool, std::optional<AwaiterResultType>>;
};

template <typename Impl, typename Suspender = FiberSuspender> class AwaiterBase {
protected:
  explicit AwaiterBase() = default;
  ~AwaiterBase() = default;

  AwaiterBase(const AwaiterBase&) = delete;
  AwaiterBase& operator=(const AwaiterBase&) = delete;
  AwaiterBase(AwaiterBase&&) = delete;
  AwaiterBase& operator=(AwaiterBase&&) = delete;

public:
  using AwaiterBaseImpl = Impl;

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
    Suspender::DoSuspend(self._Owner.value().get(), caller);
    self.DoAwaitSuspend();
  }

private:
  std::optional<std::reference_wrapper<Fiber>> _Owner;
};

} // namespace Fiber
} // namespace Omni
