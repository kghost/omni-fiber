#pragma once

#include <queue>

#include "FiberAwaitableCustom.hpp"
#include "SharedAwaitable.hpp"

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

  void AwaitValue() {}
  bool AwaitReady() const { return !_Queue.empty(); }

  OMNIFIBER_API FiberAwaitableCustom<EventQueue<Element>, SharedAwaitable> operator co_await() {
    return FiberAwaitableCustom<EventQueue<Element>, SharedAwaitable>(_AwaitContext, *this);
  }

  OMNIFIBER_API bool IsEmpty() const { return _Queue.empty(); }

  OMNIFIBER_API void Push(Element& element) {
    _Queue.push(element);
    SharedAwaitable::Fire(_AwaitContext);
  }

  OMNIFIBER_API void Push(Element&& element) {
    _Queue.emplace(std::forward<Element>(element));
    SharedAwaitable::Fire(_AwaitContext);
  }

  OMNIFIBER_API Element PopFront() {
    Element front = std::move(_Queue.front());
    _Queue.pop();
    return front;
  }

private:
  std::queue<Element> _Queue;
  SharedAwaitable::ContextStorage _AwaitContext;
};

} // namespace Fiber
} // namespace Omni
