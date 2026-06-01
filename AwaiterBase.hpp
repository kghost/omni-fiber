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

template <typename Suspender = FiberSuspender> class AwaiterBase {
protected:
  explicit AwaiterBase() = default;
  ~AwaiterBase() = default;

  AwaiterBase(const AwaiterBase&) = delete;
  AwaiterBase& operator=(const AwaiterBase&) = delete;
  AwaiterBase(AwaiterBase&&) = delete;
  AwaiterBase& operator=(AwaiterBase&&) = delete;

public:
  using AwaiterBaseImpl = Suspender;

  bool IsSuspended() const noexcept { return _OwnerFiber.has_value(); }
  void SetOwnerPromise(Fiber& owner) { _OwnerFiber = owner; }
  Fiber& GetOwnerPromise() const { return _OwnerFiber.value().get(); }
  void Schedule() { _OwnerFiber.value().get().Schedule(); }

  template <typename PromiseType> void DoAwaitSuspend(std::coroutine_handle<PromiseType> caller) {
    auto& promise = caller.promise();
    _OwnerFiber = promise.GetFiber();
#ifndef NDEBUG
    promise.SetInstructionPointer(__builtin_return_address(0));
    _OwnerFiber.value().get().SetSuspendedPromise(&promise);
#endif
    Suspender::DoSuspend(_OwnerFiber.value().get(), caller);
  }

private:
  std::optional<std::reference_wrapper<Fiber>> _OwnerFiber;
};

} // namespace Fiber
} // namespace Omni
