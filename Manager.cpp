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

Manager::~Manager() { assert(_RootFiber->IsFinished()); }

void Manager::Schedule(std::shared_ptr<Fiber> fiber) {
  _ReadyQueue.push(fiber);
  if (!Executing && !Posted) {
    _Executor.Post(*this);
  }
}

void Manager::Run() {
  while (!_ReadyQueue.empty()) {
    std::weak_ptr<Fiber> weak = _ReadyQueue.front();
    _ReadyQueue.pop();
    if (auto fiber = weak.lock()) {
      fiber->Resume();
    }
  }

  if (_RootFiber->IsFinished() && _RootFiber->_Exception.has_value()) {
    throw FiberException{.Fiber = _RootFiber, .InnerException = _RootFiber->_Exception.value()};
  }
}

void Manager::DumpAllFibers() { _RootFiber->DumpAllFibers(Log, 0); }

void Manager::OnFiberFinished(std::shared_ptr<Fiber> /*unused*/) {
  BOOST_LOG_SEV(Log, boost::log::trivial::severity_level::debug) << "All Fiber Finished.";
}

} // namespace Fiber
} // namespace Omni