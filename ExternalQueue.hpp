#pragma once

#include <mutex>
#include <queue>
#include <utility>

#include <boost/asio.hpp>

#include "AwaiterCustom.hpp"
#include "SharedAwaiter.hpp"

namespace Omni::Fiber {

class Fiber;

template <typename Element> class ExternalQueue {
public:
  explicit ExternalQueue(boost::asio::any_io_executor executor) : _Executor(std::move(executor)) {}
  ~ExternalQueue() = default;

  ExternalQueue(ExternalQueue&) = delete;
  auto operator=(ExternalQueue&) -> ExternalQueue& = delete;
  ExternalQueue(ExternalQueue&&) = delete;
  auto operator=(ExternalQueue&&) -> ExternalQueue& = delete;

  void AwaitValue() {}

  [[nodiscard]] auto AwaitReady() const -> bool {
    std::lock_guard<std::mutex> lock(_Mutex);
    return !_Queue.empty();
  }

  auto operator co_await() -> AwaiterCustom<ExternalQueue<Element>, SharedAwaiter> {
    return AwaiterCustom<ExternalQueue<Element>, SharedAwaiter>(_AwaitContext, *this);
  }

  [[nodiscard]] auto IsEmpty() const -> bool {
    std::lock_guard<std::mutex> lock(_Mutex);
    return _Queue.empty();
  }

  void Push(const Element& element) {
    {
      std::lock_guard<std::mutex> lock(_Mutex);
      _Queue.push(element);
    }
    Notify();
  }

  void Push(Element&& element) {
    {
      std::lock_guard<std::mutex> lock(_Mutex);
      _Queue.push(std::move(element));
    }
    Notify();
  }

  void Push(auto&& element) {
    {
      std::lock_guard<std::mutex> lock(_Mutex);
      _Queue.emplace(std::forward<decltype(element)>(element));
    }
    Notify();
  }

  auto PopFront() -> Element {
    std::lock_guard<std::mutex> lock(_Mutex);
    assert(!_Queue.empty());
    Element front = std::move(_Queue.front());
    _Queue.pop();
    return front;
  }

private:
  void Notify() {
    boost::asio::post(_Executor, [context = _AwaitContext]() mutable -> void {
      SharedAwaiter::Fire(context);
    });
  }

  mutable std::mutex _Mutex;
  std::queue<Element> _Queue;
  boost::asio::any_io_executor _Executor;
  SharedAwaiter::ContextStorage _AwaitContext;
};

} // namespace Omni::Fiber
