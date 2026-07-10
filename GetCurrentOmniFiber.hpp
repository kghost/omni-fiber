#pragma once

#include <coroutine>

#include "Fiber.hpp"

namespace Omni::Fiber {

class GetCurrentOmniFiber {
public:
  // NOLINTNEXTLINE(readability-convert-member-functions-to-static)
  [[nodiscard]] constexpr auto await_ready() noexcept -> bool { return false; }

  template <typename PromiseType> auto await_suspend(std::coroutine_handle<PromiseType> caller) noexcept -> bool {
    _Fiber = &caller.promise().GetFiber();
    return false;
  }

  [[nodiscard]] auto await_resume() const noexcept -> Fiber& { return *_Fiber; }

private:
  Fiber* _Fiber = nullptr;
};

} // namespace Omni::Fiber
