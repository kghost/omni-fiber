#pragma once

#include <queue>

#include "FiberAwaitContext.hpp"
#include "FiberAwaitableCustom.hpp"

#include "shared.h"

namespace Omni {
namespace Fiber {

class Fiber;

template <typename Element> class EventQueue {
public:
  OMNIFIBER_API explicit EventQueue() = default;

  EventQueue(EventQueue&) = delete;
  EventQueue& operator=(EventQueue&) = delete;
  EventQueue(EventQueue&&) = delete;
  EventQueue& operator=(EventQueue&&) = delete;

  using AwaitResultType = void;
  void AwaitValue() {}
  bool AwaitReady() const { return !_Queue.empty(); }

  OMNIFIBER_API FiberAwaitableCustom<EventQueue<Element>> operator co_await() {
    return FiberAwaitableCustom<EventQueue<Element>>{FiberAwaitContext::Get(_AwaitContext), *this};
  }

  OMNIFIBER_API bool IsEmpty() const { return _Queue.empty(); }

  OMNIFIBER_API void Push(Element& element) {
    _Queue.push(element);
    if (auto awaitContext = _AwaitContext.lock()) {
      awaitContext->Fire();
    }
  }

  OMNIFIBER_API void Push(Element&& element) {
    _Queue.emplace(std::forward<Element>(element));
    if (auto awaitContext = _AwaitContext.lock()) {
      awaitContext->Fire();
    }
  }

  OMNIFIBER_API Element PopFront() {
    Element front = std::move(_Queue.front());
    _Queue.pop();
    return front;
  }

private:
  std::queue<Element> _Queue;
  std::weak_ptr<FiberAwaitContext> _AwaitContext;
};

} // namespace Fiber
} // namespace Omni
