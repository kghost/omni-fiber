#pragma once

#include "ManagerDeclare.h"

#include "Fiber.h"

namespace Omni {
namespace Fiber {

template <typename CoroutineFunction>
requires std::is_invocable_r_v<Coroutine<void>, CoroutineFunction> std::shared_ptr<Fiber>
Manager::SpawnRoot(std::string &&name, CoroutineFunction &&function) {
  _RootFiber.reset(new Fiber(*this, std::move(name), *this, std::forward<CoroutineFunction>(function)));
  _RootFiber->Schedule();
  return _RootFiber;
}

} // namespace Fiber
} // namespace Omni
