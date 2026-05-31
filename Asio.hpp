#pragma once

#include <memory>
#include <utility>

#include <boost/asio.hpp>

#include "Executor.hpp"
#include "Fiber.hpp"
#include "Manager.hpp"
#include "OneshotEvent.hpp"

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

template <typename... Results> class AsioResult {
public:
  explicit AsioResult() : _Event(std::make_shared<OneshotEvent<std::tuple<Results...>>>()) {}

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
  std::shared_ptr<OneshotEvent<std::tuple<Results...>>> _Event;
};

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