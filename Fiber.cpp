#include "Fiber.hpp"

#include <boost/describe/enum_to_string.hpp>
#include <cassert>
#include <optional>
#include <utility>

#include "AwaiterAlwaysSuspend.hpp"
#include "FiberException.hpp"
#include "GetCurrentOmniFiber.hpp"
#include "Manager.hpp"
#include "SharedAwaitContext.hpp"
#include "SharedAwaiter.hpp"

#ifndef NDEBUG
#include "SymbolResolver.hpp"
#endif

namespace Omni {
namespace Fiber {

void Fiber::OnChildFinished(Fiber& child) {
  auto it = _Children.find(child);
  assert(it != _Children.end());
  _FinishedChildren.insert(*it);
  _Children.erase(it);
  SharedAwaiter::Fire(_JoinAwaitContext);
}

AwaiterAlwaysSuspend<SharedAwaiter> Fiber::ChildAwaitor() {
  return AwaiterAlwaysSuspend<SharedAwaiter>(_JoinAwaitContext);
}

Coroutine<void> Fiber::Wait(std::function<bool()> until) {
  assert(&co_await GetCurrentOmniFiber() == this);
  while (!until()) {
    co_await ChildAwaitor();
  }
}

bool Fiber::TryJoin(std::shared_ptr<Fiber> child) {
  assert(_Children.contains(child) || _FinishedChildren.contains(child));
  auto it = _FinishedChildren.find(child);
  if (it != _FinishedChildren.end()) {
    _FinishedChildren.erase(it);
    if (child->_Exception.has_value()) {
      throw FiberException{._Fiber = child, ._InnerException = child->_Exception.value()};
    }
    return true;
  } else {
    return false;
  }
}

Coroutine<void> Fiber::Join(std::shared_ptr<Fiber> child) {
  co_await Wait([&] { return _FinishedChildren.contains(child); });
  auto joined = TryJoin(child);
  assert(joined);
  co_return;
}

Coroutine<std::shared_ptr<Fiber>> Fiber::WaitFor() {
  assert(!_Children.empty() || !_FinishedChildren.empty());
  co_await Wait([&] { return !_FinishedChildren.empty(); });
  auto it = _FinishedChildren.begin();
  auto child = *it;
  _FinishedChildren.erase(it);
  if (child->_Exception.has_value()) {
    throw FiberException{._Fiber = child, ._InnerException = child->_Exception.value()};
  }
  co_return child;
}

Coroutine<void> Fiber::WaitAll() {
  while (!(_Children.empty() && _FinishedChildren.empty())) {
    co_await WaitFor();
  }
  co_return;
}

void Fiber::Schedule() {
  assert(_State == State::Suspended || _State == State::Ready); // It can be ready when selecting multiple events
  if (_State == State::Suspended) {
    _State = State::Ready;
    _Manager.Schedule(*this);
  }
}

void Fiber::Suspend(std::coroutine_handle<> caller) {
  assert(_State == State::Running);
  assert(!_Continuation.has_value());
  _Continuation.emplace(caller);
  _State = State::Suspending;
}

void Fiber::OmniYield(std::coroutine_handle<> caller) {
  assert(_State == State::Running);
  assert(!_Continuation.has_value());
  _Continuation.emplace(caller);
  _State = State::Yielding;
  _Manager.YieldSchedule(*this);
}

void Fiber::Starting(std::coroutine_handle<Fiber::FiberFrame::Promise> caller) {
  assert(_State == State::NotStart);
  assert(!_Continuation.has_value());
  _Continuation.emplace(caller);
  _State = State::Suspended;
}

void Fiber::Resume() {
  assert(_State == State::Ready || _State == State::Yielded);
  _State = State::Running;
#ifndef NDEBUG
  _SuspendedPromise = nullptr;
#endif
  std::exchange(_Continuation, std::nullopt).value().resume();
  switch (_State) {
  case State::Yielding:
    assert(_Continuation.has_value());
    _State = State::Yielded;
    return;
  case State::Suspending:
    assert(_Continuation.has_value());
    _State = State::Suspended;
    return;
  case State::Finishing:
    assert(!_Continuation.has_value());
    _State = State::Finished;
    _FinishNotifier.OnFiberFinished(*this);
    return;
  default:
    assert(false);
  }
}

void Fiber::Finishing() {
  assert(_State == State::Running || (_State == State::Finishing && _Exception.has_value()));
  assert(_Children.empty() && _FinishedChildren.empty());
  _State = State::Finishing;
}

void Fiber::SetException(std::exception_ptr eptr) {
  assert(_State == State::Running);
  assert(_Children.empty());
  _Exception.emplace(eptr);
  _State = State::Finishing;
}

void Fiber::DumpAllFibers(boost::log::sources::severity_logger<boost::log::trivial::severity_level>& logger,
                          int indent) {
  BOOST_LOG_SEV(logger, boost::log::trivial::severity_level::debug) << std::string(indent, ' ') << *this;
  DumpCallStack(logger, indent + 4);
  for (auto child : _Children) {
    child->DumpAllFibers(logger, indent + 2);
  }
}

#ifndef NDEBUG
void Fiber::DumpCallStack(boost::log::sources::severity_logger<boost::log::trivial::severity_level>& logger,
                          int indent) {
  if (_SuspendedPromise) {
    FiberPromise* current = _SuspendedPromise;
    int frameIdx = 0;
    while (current) {
      void* ip = current->GetInstructionPointer();
      BOOST_LOG_SEV(logger, boost::log::trivial::severity_level::debug)
          << std::string(indent, ' ') << "#" << frameIdx++ << " " << ResolveSymbol(ip);
      current = current->GetCallerPromise();
    }
  }
}
#endif

boost::log::formatting_ostream& operator<<(boost::log::formatting_ostream& p, Fiber& fiber) {
  p << "[Fiber " << fiber._Name << " @" << &fiber << " " << boost::describe::enum_to_string(fiber._State, "Unknown")
    << "]";
  return p;
}

} // namespace Fiber
} // namespace Omni