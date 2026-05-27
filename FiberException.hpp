#pragma once

#include <exception>
#include <memory>

namespace Omni {
namespace Fiber {

class Fiber;

class FiberException {
public:
  std::shared_ptr<Fiber> _Fiber;
  std::exception_ptr _InnerException;
};

} // namespace Fiber
} // namespace Omni