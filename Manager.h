#pragma once

#include <boost/log/common.hpp>
#include <boost/log/trivial.hpp>
#include <cassert>
#include <memory>
#include <queue>
#include <string>
#include <type_traits>

#include "Coroutine.h"
#include "EventQueue.h"
#include "Executor.h"
#include "Fiber.h"
#include "FiberFinishNotifier.h"

#include "shared.h"

namespace Omni {
namespace Fiber {

class Fiber;

class Manager : public FiberFinishNotifier {
public:
  OMNIFIBER_API Manager(Executor& executor);

  Manager(const Manager&) = delete;
  Manager& operator=(Manager&) = delete;
  Manager(Manager&&) = delete;
  Manager& operator=(Manager&&) = delete;

  OMNIFIBER_API static bool HasFiberRunning() { return _CurrentFiber.lock().operator bool(); }

  OMNIFIBER_API static std::shared_ptr<Fiber> GetCurrentFiber() {
    assert(HasFiberRunning());
    return _CurrentFiber.lock();
  }

  OMNIFIBER_API void DumpAllFibers();

  OMNIFIBER_API void Schedule(std::shared_ptr<Fiber> fiber); // Mark the fiber ready to be scheduled.

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
        assert(_Manager._ReadyQueue.empty());
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

  void OnFiberFinished(std::shared_ptr<Fiber> fiber) override;

private:
  class CurrentFiberSetter {
  public:
    CurrentFiberSetter(std::shared_ptr<Fiber> current)
#ifndef NDEBUG
        : _Current(current)
#endif
    {
      assert(!Manager::HasFiberRunning());
      Manager::_CurrentFiber = current;
    }

    ~CurrentFiberSetter() {
#ifndef NDEBUG
      assert(Manager::GetCurrentFiber() == _Current);
#endif
      Manager::_CurrentFiber.reset();
    }

  private:
#ifndef NDEBUG
    std::shared_ptr<Fiber> _Current;
#endif
  };

  OMNIFIBER_API void Run(); // Run until all fibers are not ready.

  Executor& _Executor;
  bool Posted = false;
  bool Executing = false;

  std::queue<std::weak_ptr<Fiber>> _ReadyQueue;
  std::shared_ptr<Fiber> _RootFiber;

  static std::weak_ptr<Fiber> _CurrentFiber;

  boost::log::sources::severity_logger<boost::log::trivial::severity_level> Log;
};

} // namespace Fiber
} // namespace Omni
