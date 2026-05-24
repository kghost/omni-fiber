#include "ManagerDeclare.h"

#include <boost/format.hpp>
#include <boost/log/attributes/constant.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/trivial.hpp>

#include "Fiber.h"

namespace Omni {
namespace Fiber {

std::weak_ptr<Fiber> Manager::_CurrentFiber;

Manager::Manager(Executor& executor) : _Executor(executor) {
  Log.add_attribute("Component", boost::log::attributes::constant<std::string>(
                                     (boost::format("%1%(%2%)") % typeid(*this).name() % this).str()));
}

void Manager::Schedule(std::shared_ptr<Fiber> fiber) {
  _ReadyQueue.push(fiber);
  if (!Executing && !Posted) {
    _Executor.Post(*this);
  }
}

void Manager::Run() {
  BOOST_LOG_SEV(Log, boost::log::trivial::severity_level::debug) << std::string(10, '=') << " Before Run";
  DumpAllFibers();
  BOOST_LOG_SEV(Log, boost::log::trivial::severity_level::debug) << std::string(10, '=') << " Before Run";

  assert(!HasFiberRunning());

  while (!_ReadyQueue.empty()) {
    std::weak_ptr<Fiber> weak = _ReadyQueue.front();
    _ReadyQueue.pop();
    if (auto fiber = weak.lock()) {
      auto setter = CurrentFiberSetter(fiber);
      fiber->Resume();
    }
  }

  BOOST_LOG_SEV(Log, boost::log::trivial::severity_level::debug) << std::string(10, '=') << " After Run";
  DumpAllFibers();
  BOOST_LOG_SEV(Log, boost::log::trivial::severity_level::debug) << std::string(10, '=') << " After Run";
}

void Manager::DumpAllFibers() { _RootFiber->DumpAllFibers(Log, 0); }

void Manager::OnFiberFinished(std::shared_ptr<Fiber> fiber) {
  BOOST_LOG_SEV(Log, boost::log::trivial::severity_level::debug) << "All Fiber Finished.";
}

} // namespace Fiber
} // namespace Omni