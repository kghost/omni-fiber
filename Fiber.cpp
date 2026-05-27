#include "Fiber.hpp"

#include <boost/describe/enum_to_string.hpp>
#include <cassert>
#include <optional>
#include <utility>

#include "AwaitableAlwaysSuspend.hpp"
#include "FiberException.hpp"
#include "GetCurrentFiber.hpp"
#include "Manager.hpp"
#include "SharedAwaitContext.hpp"

namespace Omni {
namespace Fiber {

void Fiber::OnChildFinished(std::shared_ptr<Fiber> child) {
  assert(_Children.contains(child));
  _Children.erase(child);
  _FinishedChildren.insert(child);
  SharedAwaitable::Fire(_JoinAwaitContext);
}

Coroutine<void> Fiber::Wait(std::function<bool()> until) {
  assert(&co_await GetCurrentFiber() == this);
  while (true) {
    if (until()) {
      co_return;
    }
    co_await AwaitableAlwaysSuspend<SharedAwaitable>(_JoinAwaitContext);
  }
}

Coroutine<void> Fiber::Join(std::shared_ptr<Fiber> child) {
  assert(_Children.contains(child) || _FinishedChildren.contains(child));
  co_await Wait([&] { return _FinishedChildren.contains(child); });
  _FinishedChildren.erase(child);
  if (child->_Exception.has_value()) {
    throw FiberException{._Fiber = child, ._InnerException = child->_Exception.value()};
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
    throw FiberException{._Fiber = ret, ._InnerException = ret->_Exception.value()};
  }
  co_return ret;
}

Coroutine<void> Fiber::WaitAll() {
  while (!(_Children.empty() && _FinishedChildren.empty())) {
    co_await WaitFor();
  }
  co_return;
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