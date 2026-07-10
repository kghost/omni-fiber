#include "Fiber.hpp"

#include <boost/describe/enum_to_string.hpp>
#include <cassert>
#include <coroutine>
#include <exception>
#include <functional>
#include <memory>
#include <optional>
#include <utility>

#include "AwaiterAlwaysSuspend.hpp"
#include "Coroutine.hpp"
#include "FiberException.hpp"
#include "GetCurrentOmniFiber.hpp"
#include "Manager.hpp"
#include "SharedAwaitContext.hpp"
#include "SharedAwaiter.hpp"

#ifndef NDEBUG
#include "SymbolResolver.hpp"
#endif

namespace Omni::Fiber {

void Fiber::OnChildFinished(Fiber& child) {
  auto iterator = _Children.find(child);
  assert(iterator != _Children.end());
  _FinishedChildren.insert(*iterator);
  _Children.erase(iterator);
  SharedAwaiter::Fire(_JoinAwaitContext);
}

auto Fiber::ChildAwaitor() -> AwaiterAlwaysSuspend<SharedAwaiter> {
  return AwaiterAlwaysSuspend<SharedAwaiter>(_JoinAwaitContext);
}

auto Fiber::Wait(std::function<bool()> until) -> Coroutine<void> {
  assert(&co_await GetCurrentOmniFiber() == this);
  while (!until()) {
    co_await ChildAwaitor();
  }
}

auto Fiber::TryJoin(const std::shared_ptr<Fiber>& child) -> bool {
  assert(_Children.contains(child) || _FinishedChildren.contains(child));
  auto iterator = _FinishedChildren.find(child);
  if (iterator != _FinishedChildren.end()) {
    _FinishedChildren.erase(iterator);
    if (child->_Exception.has_value()) {
      throw FiberException{._Fiber = child, ._InnerException = child->_Exception.value()};
    }
    return true;
  } else {
    return false;
  }
}

auto Fiber::Join(const std::shared_ptr<Fiber>& child) -> Coroutine<void> {
  co_await Wait([&] -> bool { return _FinishedChildren.contains(child); });
  auto joined = TryJoin(child);
  assert(joined);
  co_return;
}

auto Fiber::WaitFor() -> Coroutine<std::shared_ptr<Fiber>> {
  assert(!_Children.empty() || !_FinishedChildren.empty());
  co_await Wait([&] -> bool { return !_FinishedChildren.empty(); });
  auto iterator = _FinishedChildren.begin();
  auto child = *iterator;
  _FinishedChildren.erase(iterator);
  if (child->_Exception.has_value()) {
    throw FiberException{._Fiber = child, ._InnerException = child->_Exception.value()};
  }
  co_return child;
}

auto Fiber::WaitAll() -> Coroutine<void> {
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

void Fiber::SetException(const std::exception_ptr& eptr) {
  assert(_State == State::Running);
  assert(_Children.empty());
  _Exception.emplace(eptr);
  _State = State::Finishing;
}

void Fiber::DumpAllFibers(boost::log::sources::severity_logger<boost::log::trivial::severity_level>& logger,
                          int indent) {
  BOOST_LOG_SEV(logger, boost::log::trivial::severity_level::debug) << std::string(indent, ' ') << *this;
  DumpCallStack(logger, indent + 4);
  for (const auto& child : _Children) {
    child->DumpAllFibers(logger, indent + 2);
  }
}

#ifndef NDEBUG
void Fiber::DumpCallStack(boost::log::sources::severity_logger<boost::log::trivial::severity_level>& logger,
                          int indent) {
  if (_SuspendedPromise != nullptr) {
    const FiberPromise* current = _SuspendedPromise;
    int frameIdx = 0;
    while (current != nullptr) {
      void* instructionPointer = current->GetInstructionPointer();
      BOOST_LOG_SEV(logger, boost::log::trivial::severity_level::debug)
          << std::string(indent, ' ') << "#" << frameIdx++ << " " << ResolveSymbol(instructionPointer);
      current = current->GetCallerPromise();
    }
  }
}
#endif

auto operator<<(boost::log::formatting_ostream& stream, Fiber& fiber) -> boost::log::formatting_ostream& {
  stream << "[Fiber " << fiber._Name << " @" << &fiber << " "
         << boost::describe::enum_to_string(fiber._State, "Unknown") << "]";
  return stream;
}

} // namespace Omni::Fiber
