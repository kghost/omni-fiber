#pragma once

#include <coroutine>
#include <expected>
#include <functional>
#include <optional>
#include <stacktrace>

namespace Omni {
namespace Fiber {

class Fiber;
class FiberPromise;

struct FiberActionBase {
  static void Schedule(Fiber& fiber);
#ifndef NDEBUG
  static void SetSuspendedPromise(Fiber& fiber, FiberPromise& promise);
#endif
};

struct FiberSuspender : public FiberActionBase {
  static void DoSuspend(Fiber& fiber, std::coroutine_handle<> caller);
};

struct FiberYielder : public FiberActionBase {
  static void DoSuspend(Fiber& fiber, std::coroutine_handle<> caller);
};

template <typename Awaiter> struct AwaiterTraits {
  struct AwaiterNotReady {};
  using AwaiterResultType = decltype(std::declval<Awaiter>().await_resume());
  using AwaiterResultExpectedType = std::expected<AwaiterResultType, AwaiterNotReady>;
};

template <typename Suspender> class AwaiterBase {
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
  void Schedule() { Suspender::Schedule(_OwnerFiber.value().get()); }

  template <typename PromiseType>
  void DoAwaitSuspend(std::coroutine_handle<PromiseType> caller
#ifndef NDEBUG
                      ,
                      void* ip = (void*)std::stacktrace::current().at(0).native_handle()
#endif
  ) {
    auto& promise = caller.promise();
    _OwnerFiber = promise.GetFiber();
#ifndef NDEBUG
    promise.SetInstructionPointer(ip);
    Suspender::SetSuspendedPromise(_OwnerFiber.value().get(), promise);
#endif
    Suspender::DoSuspend(_OwnerFiber.value().get(), caller);
  }

private:
  std::optional<std::reference_wrapper<Fiber>> _OwnerFiber;
};

} // namespace Fiber
} // namespace Omni
