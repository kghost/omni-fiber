#pragma once

#include <optional>

#include <boost/asio.hpp>

#include "Coroutine.h"
#include "Event.h"
#include "Executor.h"
#include "Fiber.h"
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

} // namespace Fiber
} // namespace Omni

namespace boost {
namespace asio {

template <typename... Results> struct async_result<Omni::Fiber::AsioUseFiberType, void(Results...)> {
  template <typename Initiation, typename... InitArgs>
  static Omni::Fiber::Coroutine<std::tuple<Results...>> initiate(Initiation&& init, Omni::Fiber::AsioUseFiberType,
                                                                 InitArgs&&... initArgs) {
    boost::asio::cancellation_signal cancel;
    std::optional<std::tuple<Results...>> rets;
    Omni::Fiber::Event event;
    init(boost::asio::bind_cancellation_slot(cancel.slot(),
                                             [&rets, &event](Results... results) {
                                               rets.emplace(std::move(results)...);
                                               event.Set();
                                             }),
         std::forward<InitArgs>(initArgs)...);
    co_await event;
    co_return std::move(rets.value());
  }
};

} // namespace asio
} // namespace boost