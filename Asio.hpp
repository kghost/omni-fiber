#pragma once

#include <utility>

#include <boost/asio.hpp>

#include "Coroutine.hpp"
#include "Event.hpp"
#include "Executor.hpp"
#include "Fiber.hpp"
#include "Manager.hpp"

namespace Omni {
namespace Fiber {

class AsioUseFiberType {};
inline constexpr const AsioUseFiberType AsioUseFiber;

class AsioExecutor : public Executor {
public:
  explicit AsioExecutor(boost::asio::io_context& io) : Executor(), _Io(io) {}

  AsioExecutor(AsioExecutor&) = delete;
  AsioExecutor& operator=(AsioExecutor&) = delete;
  AsioExecutor(AsioExecutor&&) = delete;
  AsioExecutor& operator=(AsioExecutor&&) = delete;

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
  static Omni::Fiber::Coroutine<std::tuple<Results...>> initiate(Initiation initiation, Omni::Fiber::AsioUseFiberType,
                                                                 InitArgs... initArgs) {
    Omni::Fiber::Event<std::tuple<Results...>> event;
    initiation([&event](this auto self, Results... results) { event.Fire(std::make_tuple(std::move(results)...)); },
               std::move(initArgs)...);
    co_return co_await event;
  }
};

} // namespace asio
} // namespace boost