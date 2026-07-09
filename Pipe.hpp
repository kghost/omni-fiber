#pragma once

#include <cassert>
#include <expected>
#include <type_traits>
#include <utility>

#include <boost/log/common.hpp>
#include <boost/log/trivial.hpp>
#include <variant>

#include "AwaiterCustom.hpp"
#include "Coroutine.hpp"
#include "SharedAwaiter.hpp"

namespace Omni::Fiber {

class PipeClosed {};

// A single consumer single producer pipe.
template <typename DataType> class Pipe final {
private:
  struct Empty {};
  struct Closed {};

public:
  explicit Pipe() = default;
  ~Pipe() { assert(std::holds_alternative<Closed>(_Data) && "Pipe must be closed before destruction"); }

  Pipe(Pipe&) = delete;
  auto operator=(Pipe&) -> Pipe& = delete;
  Pipe(Pipe&&) = delete;
  auto operator=(Pipe&&) -> Pipe& = delete;

  class Producer {
  private:
    friend class Pipe;
    explicit Producer(Pipe& pipe) : _Pipe(pipe) {}

  public:
    ~Producer() {}
    Producer(Producer&) = delete;
    auto operator=(Producer&) -> Producer& = delete;
    Producer(Producer&&) = delete;
    auto operator=(Producer&&) -> Producer& = delete;

    [[nodiscard]] auto AwaitReady() const -> bool { return !std::holds_alternative<DataType>(_Pipe._Data); }
    void AwaitValue() {}

    auto Awaiter() { return AwaiterCustom<Producer, SharedAwaiter>(_Pipe._AwaitWriteContext, *this); }
    auto Put(auto&& data) && -> Coroutine<std::expected<void, PipeClosed>> {
      return PutData(std::forward<decltype(data)>(data));
    }
    auto Shutdown() && -> Coroutine<std::expected<void, PipeClosed>> { return PutData(Closed{}); }

  private:
    Pipe& _Pipe;

    auto PutData(auto&& value) -> Coroutine<std::expected<void, PipeClosed>> {
      while (std::holds_alternative<DataType>(_Pipe._Data)) {
        co_await Awaiter();
      }
      if (std::holds_alternative<Closed>(_Pipe._Data)) {
        co_return std::unexpected<PipeClosed>{PipeClosed{}};
      }
      if constexpr (std::is_same_v<std::decay_t<decltype(value)>, Closed>) {
        _Pipe._Data.template emplace<Closed>();
      } else {
        _Pipe._Data.template emplace<DataType>(std::forward<decltype(value)>(value));
      }
      SharedAwaiter::Fire(_Pipe._AwaitReadContext);
      co_return std::expected<void, PipeClosed>{};
    }
  };

  class Consumer {
  public:
    explicit Consumer(Pipe& pipe) : _Pipe(pipe) {}
    ~Consumer() {}

    Consumer(Consumer&) = delete;
    auto operator=(Consumer&) -> Consumer& = delete;
    Consumer(Consumer&&) = delete;
    auto operator=(Consumer&&) -> Consumer& = delete;

    [[nodiscard]] auto AwaitReady() const -> bool { return !std::holds_alternative<Empty>(_Pipe._Data); }
    auto AwaitValue() -> std::expected<DataType, PipeClosed> {
      assert(!std::holds_alternative<Empty>(_Pipe._Data));
      if (std::holds_alternative<DataType>(_Pipe._Data)) {
        DataType ret = std::get<DataType>(std::move(_Pipe._Data));
        _Pipe._Data = Empty{};
        SharedAwaiter::Fire(_Pipe._AwaitWriteContext);
        return ret;
      } else if (std::holds_alternative<Closed>(_Pipe._Data)) {
        return std::unexpected<PipeClosed>{PipeClosed{}};
      } else {
        std::unreachable();
      }
    }

    auto operator co_await() && -> AwaiterCustom<Consumer, SharedAwaiter> {
      return AwaiterCustom<Consumer, SharedAwaiter>(_Pipe._AwaitReadContext, *this);
    }

    void DiscardAndClose() && {
      _Pipe._Data.template emplace<Closed>();
      SharedAwaiter::Fire(_Pipe._AwaitWriteContext);
      SharedAwaiter::Fire(_Pipe._AwaitReadContext);
    }

  private:
    Pipe& _Pipe;
  };

  [[nodiscard]] auto IsClosed() const -> bool { return std::holds_alternative<Closed>(_Data); }
  auto GetProducer() -> Producer { return Producer(*this); }
  auto GetConsumer() -> Consumer { return Consumer(*this); }

private:
  SharedAwaiter::ContextStorage _AwaitReadContext;
  SharedAwaiter::ContextStorage _AwaitWriteContext;
  std::variant<Empty, Closed, DataType> _Data;
};

} // namespace Omni::Fiber
