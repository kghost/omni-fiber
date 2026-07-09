#pragma once

#include <coroutine>
#include <expected>
#include <functional>
#include <optional>
#if __has_include(<stacktrace>)
#include <stacktrace>
#endif

namespace Omni::Fiber {

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

public:
  AwaiterBase(const AwaiterBase&) = delete;
  auto operator=(const AwaiterBase&) -> AwaiterBase& = delete;
  AwaiterBase(AwaiterBase&&) = delete;
  auto operator=(AwaiterBase&&) -> AwaiterBase& = delete;

  using AwaiterBaseImpl = Suspender;

  [[nodiscard]] auto IsSuspended() const noexcept -> bool { return _OwnerFiber.has_value(); }
  void SetOwnerPromise(Fiber& owner) { _OwnerFiber = owner; }
  [[nodiscard]] auto GetOwnerPromise() const -> Fiber& { return _OwnerFiber.value().get(); }
  void Schedule() { Suspender::Schedule(_OwnerFiber.value().get()); }

  template <typename PromiseType>
  void DoAwaitSuspend(
      std::coroutine_handle<PromiseType> caller
#ifndef NDEBUG
#if __has_include(<stacktrace>)
      ,
      // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast, performance-no-int-to-ptr)
      void* instructionPointer = reinterpret_cast<void*>(std::stacktrace::current(0, 1).at(0).native_handle())
#endif
#endif
  ) {
    auto& promise = caller.promise();
    _OwnerFiber = promise.GetFiber();
#ifndef NDEBUG
#if __has_include(<stacktrace>)
    promise.SetInstructionPointer(instructionPointer);
#else
    promise.SetInstructionPointer(__builtin_return_address(0));
#endif
    Suspender::SetSuspendedPromise(_OwnerFiber.value().get(), promise);
#endif
    Suspender::DoSuspend(_OwnerFiber.value().get(), caller);
  }

private:
  std::optional<std::reference_wrapper<Fiber>> _OwnerFiber;
};

} // namespace Omni::Fiber
