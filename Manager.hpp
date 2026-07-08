#pragma once

#include <boost/log/common.hpp>
#include <boost/log/trivial.hpp>
#include <cassert>
#include <functional>
#include <memory>
#include <queue>
#include <string>
#include <type_traits>

#include "Coroutine.hpp"
#include "Executor.hpp"
#include "Fiber.hpp"
#include "FiberFinishNotifier.hpp"

namespace Omni {
namespace Fiber {

class Fiber;

class Manager final : public FiberFinishNotifier {
public:
  Manager(Executor& executor);
  ~Manager();

  Manager(const Manager&) = delete;
  Manager& operator=(Manager&) = delete;
  Manager(Manager&&) = delete;
  Manager& operator=(Manager&&) = delete;

  void DumpAllFibers();
  void Schedule(Fiber& fiber); // Mark the fiber ready to be scheduled.
  void YieldSchedule(Fiber& fiber);

  // Spawn the root fiber
  template <typename CoroutineFunction>
    requires std::is_invocable_r_v<Coroutine<void>, CoroutineFunction>
  std::shared_ptr<Fiber> SpawnRoot(std::string&& name, CoroutineFunction&& function) {
    _RootFiber.reset(new Fiber(*this, std::move(name), *this, std::forward<CoroutineFunction>(function)));
    _RootFiber->Schedule();
    return _RootFiber;
  }

  class Runner {
  public:
    Runner(Manager& manager) : _Manager(manager) { _Manager.Posted = true; }
    ~Runner() {
      if (!Moved) {
        assert(_Manager._ReadyQueue.empty() && _Manager._YieldQueue.empty());
        _Manager.Posted = false;
      }
    }

    Runner(const Runner&) = delete;
    Runner& operator=(const Runner&) = delete;
    Runner(Runner&& that) noexcept : _Manager(that._Manager) { that.Moved = true; }
    Runner& operator=(Runner&&) = delete;

    void operator()() {
      _Manager.Executing = true;
      _Manager.Run();
      _Manager.Executing = false;
    }

  private:
    bool Moved = false;
    Manager& _Manager;
  };

  Runner GetRunner() { return Runner(*this); }

  void OnFiberFinished(Fiber& fiber) override;

private:
  void Run(); // Run until all fibers are not ready.

  Executor& _Executor;
  bool Posted = false;
  bool Executing = false;

  std::queue<std::reference_wrapper<Fiber>> _ReadyQueue;
  std::queue<std::reference_wrapper<Fiber>> _YieldQueue;
  std::shared_ptr<Fiber> _RootFiber;

  boost::log::sources::severity_logger<boost::log::trivial::severity_level> Log;
};

} // namespace Fiber
} // namespace Omni
