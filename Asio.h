#pragma once

#include <optional>

#include <boost/asio.hpp>

#include "Coroutine.h"
#include "Executor.h"
#include "Fiber.h"
#include "FiberAwaitable.h"
#include "Manager.h"

namespace Omni {
namespace Fiber {

class AsioUseFiberType {};
inline constexpr const AsioUseFiberType AsioUseFiber;

class AsioExecutor : public Executor {
public:
  AsioExecutor(boost::asio::io_context& io) : _Io(io) {}
  void Post(Manager& manager) override { boost::asio::post(_Io, manager.GetRunner()); }

private:
  boost::asio::io_context& _Io;
};

template <typename... Results> class AsioEvent : public FiberAwaitable {
public:
  bool await_ready() { return _Results.has_value(); }
  void await_suspend(std::coroutine_handle<> caller) {
    _SuspendedFiber = Manager::GetCurrentFiber();
    FiberAwaitable::await_suspend(caller);
  }
  std::tuple<Results...> await_resume() {
    _CancelSignal.emit(boost::asio::cancellation_type::all);
    FiberAwaitable::await_resume();
    return std::move(_Results.value());
  }

  AsioEvent& operator co_await() { return *this; }

  boost::asio::cancellation_slot GetCancelSlot() { return _CancelSignal.slot(); }
  void Finish(std::tuple<Results...>&& results) {
    _Results.emplace(std::move(results));
    if (auto fiber = _SuspendedFiber.lock()) {
      fiber->Schedule();
    }
  }

private:
  std::weak_ptr<Fiber> _SuspendedFiber;
  boost::asio::cancellation_signal _CancelSignal;
  std::optional<std::tuple<Results...>> _Results;
};

} // namespace Fiber
} // namespace Omni

namespace boost {
namespace asio {

template <typename... Results> struct async_result<Omni::Fiber::AsioUseFiberType, void(Results...)> {
  template <typename Initiation, typename... InitArgs>
  static Omni::Fiber::Coroutine<std::tuple<Results...>> initiate(Initiation&& init, Omni::Fiber::AsioUseFiberType,
                                                                 InitArgs&&... initArgs) {
    using EventType = Omni::Fiber::AsioEvent<Results...>;
    std::shared_ptr<EventType> event = std::make_shared<Omni::Fiber::AsioEvent<Results...>>();
    init(boost::asio::bind_cancellation_slot(event->GetCancelSlot(),
                                             [event(std::weak_ptr<EventType>(event))](Results... results) {
                                               event.lock()->Finish(std::make_tuple<Results...>(std::move(results)...));
                                             }),
         std::forward<InitArgs>(initArgs)...);
    co_return co_await *event;
  }
};

} // namespace asio
} // namespace boost