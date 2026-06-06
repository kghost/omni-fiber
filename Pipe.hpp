#pragma once

#include <cassert>
#include <expected>
#include <utility>

#include <boost/log/common.hpp>
#include <boost/log/trivial.hpp>
#include <variant>

#include "AwaiterCustom.hpp"
#include "Coroutine.hpp"
#include "SharedAwaiter.hpp"

namespace Omni {
namespace Fiber {

class PipeClosed {};

// A single consumer single producer pipe.
template <typename DataType> class Pipe final {
private:
  struct Empty {};
  struct Closed {};

public:
  explicit Pipe() {}
  ~Pipe() { assert(std::holds_alternative<Closed>(_Data) && "Pipe must be closed before destruction"); }

  Pipe(Pipe&) = delete;
  Pipe& operator=(Pipe&) = delete;
  Pipe(Pipe&&) = delete;
  Pipe& operator=(Pipe&&) = delete;

  class Producer {
  private:
    friend class Pipe;
    explicit Producer(Pipe& pipe) : _Pipe(pipe) {}

  public:
    ~Producer() {}
    Producer(Producer&) = delete;
    Producer& operator=(Producer&) = delete;
    Producer(Producer&&) = delete;
    Producer& operator=(Producer&&) = delete;

    bool AwaitReady() const { return !std::holds_alternative<DataType>(_Pipe._Data); }
    void AwaitValue() {}

    auto Awaiter() { return AwaiterCustom<Producer, SharedAwaiter>(_Pipe._AwaitWriteContext, *this); }
    Coroutine<std::expected<void, PipeClosed>> Put(DataType&& data) && { return PutData(std::forward<DataType>(data)); }
    Coroutine<std::expected<void, PipeClosed>> Close() && { return PutData(Closed{}); }

  private:
    Pipe& _Pipe;

    template <typename Value> Coroutine<std::expected<void, PipeClosed>> PutData(Value&& value) {
      while (std::holds_alternative<DataType>(_Pipe._Data)) {
        co_await Awaiter();
      }
      if (std::holds_alternative<Closed>(_Pipe._Data)) {
        co_return std::unexpected<PipeClosed>{PipeClosed{}};
      }
      _Pipe._Data = std::forward<Value>(value);
      SharedAwaiter::Fire(_Pipe._AwaitReadContext);
      co_return std::expected<void, PipeClosed>{};
    }
  };

  class Consumer {
  private:
    friend class Pipe;
    explicit Consumer(Pipe& pipe) : _Pipe(pipe) {}

  public:
    ~Consumer() {}
    Consumer(Consumer&) = delete;
    Consumer& operator=(Consumer&) = delete;
    Consumer(Consumer&&) = delete;
    Consumer& operator=(Consumer&&) = delete;

    bool AwaitReady() const { return !std::holds_alternative<Empty>(_Pipe._Data); }
    std::expected<DataType, PipeClosed> AwaitValue() {
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

    AwaiterCustom<Consumer, SharedAwaiter> operator co_await() && {
      return AwaiterCustom<Consumer, SharedAwaiter>(_Pipe._AwaitReadContext, *this);
    }

  private:
    Pipe& _Pipe;
  };

  bool IsClosed() const { return std::holds_alternative<Closed>(_Data); }
  Producer GetProducer() { return Producer(*this); }
  Consumer GetConsumer() { return Consumer(*this); }

private:
  SharedAwaiter::ContextStorage _AwaitReadContext;
  SharedAwaiter::ContextStorage _AwaitWriteContext;
  std::variant<Empty, Closed, DataType> _Data;
};

} // namespace Fiber
} // namespace Omni