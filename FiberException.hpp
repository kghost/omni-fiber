#pragma once

#include <exception>
#include <memory>

namespace Omni {
namespace Fiber {

class Fiber;

class FiberException {
public:
  std::shared_ptr<Fiber> Fiber;
  std::exception_ptr InnerException;
};

} // namespace Fiber
} // namespace Omni