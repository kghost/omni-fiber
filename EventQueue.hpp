#pragma once

#include <queue>

#include "AwaiterCustom.hpp"
#include "SharedAwaiter.hpp"

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

  OMNIFIBER_API AwaiterCustom<EventQueue<Element>, SharedAwaiter> operator co_await() {
    return AwaiterCustom<EventQueue<Element>, SharedAwaiter>(_AwaitContext, *this);
  }

  OMNIFIBER_API bool IsEmpty() const { return _Queue.empty(); }

  OMNIFIBER_API void Push(Element& element) {
    _Queue.push(element);
    SharedAwaiter::Fire(_AwaitContext);
  }

  OMNIFIBER_API void Push(Element&& element) {
    _Queue.emplace(std::forward<Element>(element));
    SharedAwaiter::Fire(_AwaitContext);
  }

  OMNIFIBER_API Element PopFront() {
    Element front = std::move(_Queue.front());
    _Queue.pop();
    return front;
  }

private:
  std::queue<Element> _Queue;
  SharedAwaiter::ContextStorage _AwaitContext;
};

} // namespace Fiber
} // namespace Omni
