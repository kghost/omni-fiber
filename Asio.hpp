#pragma once

#include <optional>
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
  static Omni::Fiber::Coroutine<std::tuple<Results...>> initiate(Initiation&& init, Omni::Fiber::AsioUseFiberType,
                                                                 InitArgs&&... initArgs) {
    auto helper = [](std::decay_t<Initiation> init,
                     std::decay_t<InitArgs>... initArgs) -> Omni::Fiber::Coroutine<std::tuple<Results...>> {
      std::optional<std::tuple<Results...>> rets;
      Omni::Fiber::Event<> event;
      init(
          [&rets, &event](Results... results) {
            rets.emplace(std::move(results)...);
            event.Fire();
          },
          std::move(initArgs)...);
      co_await event;
      co_return std::move(rets.value());
    };
    return helper(std::forward<Initiation>(init), std::forward<InitArgs>(initArgs)...);
  }
};

} // namespace asio
} // namespace boost