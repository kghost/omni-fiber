#include "Coroutine.hpp"
#include "FiberPromise.hpp"

#ifndef NDEBUG
#include <boost/log/common.hpp>
#include <boost/log/trivial.hpp>
#endif

#include "Fiber.hpp"

namespace Omni {
namespace Fiber {

#ifndef NDEBUG
void DebugOutputFiberCallStack(Fiber& fiber, FiberPromise& promise) {
  fiber.SetSuspendedPromise(&promise);
  boost::log::sources::severity_logger<boost::log::trivial::severity_level> logger;
  BOOST_LOG_SEV(logger, boost::log::trivial::severity_level::error)
      << "Unhandled exception in fiber " << fiber.GetName();
  fiber.DumpCallStack(logger, 0);
  fiber.SetSuspendedPromise(nullptr);
}
#endif

} // namespace Fiber
} // namespace Omni
