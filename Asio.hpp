#pragma once

#include <memory>
#include <utility>

#include <boost/asio.hpp>

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
  explicit AsioExecutor(boost::asio::any_io_executor executor) : Executor(), _Executor(executor) {}

  AsioExecutor(AsioExecutor&) = delete;
  AsioExecutor& operator=(AsioExecutor&) = delete;
  AsioExecutor(AsioExecutor&&) = delete;
  AsioExecutor& operator=(AsioExecutor&&) = delete;

  void Post(Manager& manager) override { boost::asio::post(_Executor, manager.GetRunner()); }

private:
  boost::asio::any_io_executor _Executor;
};

template <typename... Results> class AsioResult {
public:
  explicit AsioResult() : _Event(std::make_shared<Event<std::tuple<Results...>>>()) {}

  AsioResult(const AsioResult&) = default;
  AsioResult& operator=(const AsioResult&) = default;
  AsioResult(AsioResult&& other) noexcept : _Event(std::move(other._Event)) {}
  AsioResult& operator=(AsioResult&& other) noexcept {
    if (this != &other) {
      _Event = std::move(other._Event);
    }
    return *this;
  }

  template <typename... Args> void operator()(Args&&... args) const {
    _Event->Fire(std::make_tuple(std::forward<Args>(args)...));
  }
  decltype(auto) operator co_await() { return _Event->operator co_await(); }

private:
  std::shared_ptr<Event<std::tuple<Results...>>> _Event;
};

template <typename Function> decltype(auto) AsioApply(Function&& func) {
  return [func = std::forward<Function>(func)](auto&& args) -> decltype(auto) {
    return std::apply(func, std::move(args));
  };
}

} // namespace Fiber
} // namespace Omni

namespace boost {
namespace asio {

template <typename... Results> struct async_result<Omni::Fiber::AsioUseFiberType, void(Results...)> {
  template <typename Initiation, typename... InitArgs>
  static Omni::Fiber::AsioResult<Results...> initiate(Initiation&& initiation, Omni::Fiber::AsioUseFiberType,
                                                      InitArgs&&... initArgs) {
    Omni::Fiber::AsioResult<Results...> result;
    // enforcing explicit copy semantics for result to be stored inside asio initiate
    initiation(static_cast<decltype(result)>(result), std::forward<InitArgs>(initArgs)...);
    return result;
  }
};

} // namespace asio
} // namespace boost