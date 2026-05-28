#include "Manager.hpp"

#include <boost/format.hpp>
#include <boost/log/attributes/constant.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/trivial.hpp>

#include "Fiber.hpp"
#include "FiberException.hpp"

namespace Omni {
namespace Fiber {

Manager::Manager(Executor& executor) : _Executor(executor) {
  Log.add_attribute("Component", boost::log::attributes::constant<std::string>(
                                     (boost::format("%1%(%2%)") % typeid(*this).name() % this).str()));
}

Manager::~Manager() {
#ifndef NDEBUG
  if (!_RootFiber->IsFinished()) {
    DumpAllFibers();
  }
#endif
  assert(_RootFiber->IsFinished());
}

void Manager::Schedule(Fiber& fiber) {
  _ReadyQueue.push(fiber);
  if (!Executing && !Posted) {
    _Executor.Post(*this);
  }
}

void Manager::Run() {
  while (!_ReadyQueue.empty()) {
    auto fiber = _ReadyQueue.front();
    _ReadyQueue.pop();
    fiber.get().Resume();
  }

  if (_RootFiber->IsFinished() && _RootFiber->_Exception.has_value()) {
    throw FiberException{._Fiber = _RootFiber, ._InnerException = _RootFiber->_Exception.value()};
  }
}

void Manager::DumpAllFibers() { _RootFiber->DumpAllFibers(Log, 0); }

void Manager::OnFiberFinished(Fiber& /*unused*/) {
  BOOST_LOG_SEV(Log, boost::log::trivial::severity_level::debug) << "All Fiber Finished.";
}

} // namespace Fiber
} // namespace Omni