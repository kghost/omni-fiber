#include "Fiber.h"

#include <boost/describe/enum_to_string.hpp>
#include <cassert>
#include <optional>
#include <utility>

#include "Event.h"
#include "FiberException.hpp"
#include "GetCurrentFiber.hpp"
#include "Manager.h"

namespace Omni {
namespace Fiber {

OMNIFIBER_API Coroutine<void> Fiber::Wait(std::function<bool()> until) {
  assert(&co_await GetCurrentFiber() == this);
  if (until()) {
    co_return;
  }
  while (true) {
    while (!_ChildSignals.IsEmpty()) {
      std::shared_ptr<Fiber> finished = _ChildSignals.PopFront();
      assert(finished->IsFinished());
      assert(_Children.contains(finished));
      _Children.erase(finished);
      _FinishedChildren.insert(finished);
    }
    if (until()) {
      co_return;
    }
    co_await _ChildSignals;
    assert(!_ChildSignals.IsEmpty());
  }
}

Coroutine<void> Fiber::Join(std::shared_ptr<Fiber> child) {
  assert(_Children.contains(child) || _FinishedChildren.contains(child));
  co_await Wait([&] { return _FinishedChildren.contains(child); });
  _FinishedChildren.erase(child);
  if (child->_Exception.has_value()) {
    throw FiberException{.Fiber = child, .InnerException = child->_Exception.value()};
  }
  co_return;
}

Coroutine<std::shared_ptr<Fiber>> Fiber::WaitFor() {
  assert(!_Children.empty() || !_FinishedChildren.empty());
  co_await Wait([&] { return !_FinishedChildren.empty(); });
  auto it = _FinishedChildren.begin();
  auto ret = *it;
  _FinishedChildren.erase(it);
  if (ret->_Exception.has_value()) {
    throw FiberException{.Fiber = ret, .InnerException = ret->_Exception.value()};
  }
  co_return ret;
}

void Fiber::Schedule() {
  assert(_State == State::Suspended);
  _State = State::Ready;
  _Manager.Schedule(shared_from_this());
}

void Fiber::Suspend(std::coroutine_handle<> caller) {
  assert(_State == State::Running);
  assert(!_Continuation.has_value());
  _Continuation.emplace(caller);
  _State = State::Suspending;
}

void Fiber::StartingYield(std::coroutine_handle<> caller) {
  assert(_State == State::NotStart);
  assert(!_Continuation.has_value());
  _Continuation.emplace(caller);
  _State = State::Suspended;
}

void Fiber::Resume() {
  assert(_State == State::Ready);
  _State = State::Running;
  std::exchange(_Continuation, std::nullopt).value().resume();
  switch (_State) {
  case State::Suspending:
    assert(_Continuation.has_value());
    _State = State::Suspended;
    return;
  case State::Finishing:
    assert(!_Continuation.has_value());
    _State = State::Finished;
    _FinishNotifier.OnFiberFinished(shared_from_this());
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
  for (auto child : _Children) {
    child->DumpAllFibers(logger, indent + 2);
  }
}

boost::log::formatting_ostream& operator<<(boost::log::formatting_ostream& p, Fiber& fiber) {
  p << "[Fiber " << fiber._Name << " @" << &fiber << " " << boost::describe::enum_to_string(fiber._State, "Unknown")
    << "]";
  return p;
}

} // namespace Fiber
} // namespace Omni