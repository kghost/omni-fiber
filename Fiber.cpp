#include "../common/header.h"

#include "Fiber.h"

#include <boost/describe/enum_to_string.hpp>
#include <cassert>

#include "Event.h"
#include "Manager.h"

namespace Omni {
namespace Fiber {

Coroutine<void> Fiber::Join(std::shared_ptr<Fiber> child) {
  assert(_Children.contains(child));

  if (child->IsFinished()) {
    _Children.erase(child);
    co_return;
  }

  while (!child->IsFinished()) {
    co_await _ChildSignals;
    while (!_ChildSignals.IsEmpty()) {
      std::shared_ptr<Fiber> finished = _ChildSignals.PopFront();
      assert(finished->IsFinished());
      if (finished == child) {
        _Children.erase(child);
        co_return;
      }
    }
  }
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
  std::coroutine_handle<> cont = _Continuation.value();
  _Continuation.reset();
  cont.resume();
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
  assert(_State == State::Running);
  assert(_Children.empty());

  _State = State::Finishing;
}

void Fiber::SetException(std::exception_ptr eptr) {
  assert(_State == State::Running);
  assert(_Children.empty());

  _Exception.emplace(eptr);
  _State = State::Finishing;
}

void Fiber::DumpAllFibers(
    boost::log::sources::severity_logger<boost::log::trivial::severity_level> &logger, int indent) {
  BOOST_LOG_SEV(logger, boost::log::trivial::severity_level::debug) << std::string(indent, ' ') << *this;
  for (auto child : _Children)
    child->DumpAllFibers(logger, indent + 2);
}

boost::log::formatting_ostream &operator<<(boost::log::formatting_ostream &p, Fiber &fiber) {
  p << "[Fiber " << fiber._Name << " @" << &fiber << " " << boost::describe::enum_to_string(fiber._State, "Unknown")
    << "]";
  return p;
}

} // namespace Fiber
} // namespace Omni