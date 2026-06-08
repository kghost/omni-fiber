#include "Coroutine.hpp"
#include "FiberPromise.hpp"

#ifndef NDEBUG
#include <boost/exception/diagnostic_information.hpp>
#include <boost/log/common.hpp>
#include <boost/log/trivial.hpp>
#endif

#include "Fiber.hpp"

namespace Omni {
namespace Fiber {

#ifndef NDEBUG
void DebugOutputFiberCallStack(Fiber& fiber, FiberPromise& promise, std::exception_ptr eptr) {
  fiber.SetSuspendedPromise(&promise);
  boost::log::sources::severity_logger<boost::log::trivial::severity_level> logger;
  std::string exceptionInfo = "Unknown exception";
  if (eptr) {
    try {
      std::rethrow_exception(eptr);
    } catch (...) {
      exceptionInfo = boost::current_exception_diagnostic_information();
    }
  }
  BOOST_LOG_SEV(logger, boost::log::trivial::severity_level::error)
      << "Unhandled exception in fiber " << fiber.GetName() << ": " << exceptionInfo;
  fiber.DumpCallStack(logger, 0);
  fiber.SetSuspendedPromise(nullptr);
}
#endif

} // namespace Fiber
} // namespace Omni
