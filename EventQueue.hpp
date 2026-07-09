#pragma once

#include <queue>

#include "AwaiterCustom.hpp"
#include "SharedAwaiter.hpp"

namespace Omni::Fiber {

class Fiber;

template <typename Element> class EventQueue {
public:
  explicit EventQueue() = default;
  ~EventQueue() = default;

  EventQueue(EventQueue&) = delete;
  auto operator=(EventQueue&) -> EventQueue& = delete;
  EventQueue(EventQueue&&) = delete;
  auto operator=(EventQueue&&) -> EventQueue& = delete;

  void AwaitValue() {}
  [[nodiscard]] auto AwaitReady() const -> bool { return !_Queue.empty(); }

  auto operator co_await() -> AwaiterCustom<EventQueue<Element>, SharedAwaiter> {
    return AwaiterCustom<EventQueue<Element>, SharedAwaiter>(_AwaitContext, *this);
  }

  [[nodiscard]] auto IsEmpty() const -> bool { return _Queue.empty(); }

  void Push(Element& element) {
    _Queue.push(element);
    SharedAwaiter::Fire(_AwaitContext);
  }

  void Push(auto&& element) {
    _Queue.emplace(std::forward<decltype(element)>(element));
    SharedAwaiter::Fire(_AwaitContext);
  }

  auto PopFront() -> Element {
    Element front = std::move(_Queue.front());
    _Queue.pop();
    return front;
  }

private:
  std::queue<Element> _Queue;
  SharedAwaiter::ContextStorage _AwaitContext;
};

} // namespace Omni::Fiber
