#pragma once

#include <memory>
#include <utility>

#include <boost/asio.hpp>

#include "Event.hpp"
#include "Executor.hpp"
#include "Fiber.hpp"
#include "Manager.hpp"

namespace Omni::Fiber {

class AsioUseFiberType {};
inline constexpr const AsioUseFiberType AsioUseFiber;

class AsioExecutor : public Executor {
public:
  explicit AsioExecutor(boost::asio::any_io_executor executor) : _Executor(std::move(executor)) {}
  ~AsioExecutor() = default;

  AsioExecutor(AsioExecutor&) = delete;
  auto operator=(AsioExecutor&) -> AsioExecutor& = delete;
  AsioExecutor(AsioExecutor&&) = delete;
  auto operator=(AsioExecutor&&) -> AsioExecutor& = delete;

  void Post(Manager& manager) override { boost::asio::post(_Executor, manager.GetRunner()); }

private:
  boost::asio::any_io_executor _Executor;
};

template <typename... Results> class AsioResult {
public:
  explicit AsioResult() : _Event(std::make_shared<Event<std::tuple<Results...>>>()) {}
  ~AsioResult() = default;

  AsioResult(const AsioResult&) = default;
  auto operator=(const AsioResult&) -> AsioResult& = default;
  AsioResult(AsioResult&& other) noexcept : _Event(std::move(other._Event)) {}
  auto operator=(AsioResult&& other) noexcept -> AsioResult& {
    if (this != &other) {
      _Event = std::move(other._Event);
    }
    return *this;
  }

  template <typename... Args> void operator()(Args&&... args) const {
    _Event->Fire(std::make_tuple(std::forward<Args>(args)...));
  }
  auto operator co_await() -> decltype(auto) { return _Event->operator co_await(); }

private:
  std::shared_ptr<Event<std::tuple<Results...>>> _Event;
};

template <typename Function> auto AsioApply(Function&& func) -> decltype(auto) {
  return [func = std::forward<Function>(func)](auto&& args) -> decltype(auto) {
    return std::apply(func, std::forward<decltype(args)>(args));
  };
}

} // namespace Omni::Fiber

namespace boost::asio {

template <typename... Results> struct async_result<Omni::Fiber::AsioUseFiberType, void(Results...)> {
  static auto initiate(auto&& initiation, Omni::Fiber::AsioUseFiberType /*unused*/, auto&&... initArgs)
      -> Omni::Fiber::AsioResult<Results...> {
    Omni::Fiber::AsioResult<Results...> result;
    // enforcing explicit copy semantics for result to be stored inside asio initiate
    initiation(static_cast<decltype(result)>(result), std::forward<decltype(initArgs)>(initArgs)...);
    return result;
  }
};

} // namespace boost::asio
