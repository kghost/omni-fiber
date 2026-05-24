#pragma once

namespace Omni {
namespace Fiber {

class Manager;

class Executor {
public:
  virtual ~Executor() = default;
  virtual void Post(Manager &manager) = 0;
};

} // namespace Fiber
} // namespace Omni