#pragma once

namespace Omni::Fiber {

class Manager;

class Executor {
public:
  explicit Executor() = default;
  virtual ~Executor() = default;

  Executor(Executor&) = delete;
  auto operator=(Executor&) -> Executor& = delete;
  Executor(Executor&&) = delete;
  auto operator=(Executor&&) -> Executor& = delete;

  virtual void Post(Manager& manager) = 0;
};

} // namespace Omni::Fiber
