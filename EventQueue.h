#pragma once

#include <queue>

#include "Event.h"

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

  OMNIFIBER_API Event& operator co_await() { return _Event.operator co_await(); }

  OMNIFIBER_API bool IsEmpty() const { return _Queue.empty(); }

  OMNIFIBER_API void Push(Element& element) {
    _Queue.push(element);
    _Event.Set();
  }

  OMNIFIBER_API void Push(Element&& element) {
    _Queue.emplace(std::forward<Element>(element));
    _Event.Set();
  }

  OMNIFIBER_API Element PopFront() {
    Element front = std::move(_Queue.front());
    _Queue.pop();
    if (_Queue.empty()) {
      _Event.Reset();
    }
    return front;
  }

private:
  std::queue<Element> _Queue;
  Event _Event;
};

} // namespace Fiber
} // namespace Omni
