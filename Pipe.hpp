#pragma once

#include <expected>
#include <optional>
#include <utility>

#include <boost/log/common.hpp>
#include <boost/log/trivial.hpp>

#include "AwaiterCustom.hpp"
#include "Coroutine.hpp"
#include "SingleAwaitContext.hpp"
#include "SingleAwaiter.hpp"

namespace Omni {
namespace Fiber {

// A single consumer single producer pipe.
template <typename DataType> class Pipe final {
public:
  explicit Pipe() {}
  ~Pipe() {}

  Pipe(Pipe&) = delete;
  Pipe& operator=(Pipe&) = delete;
  Pipe(Pipe&&) = delete;
  Pipe& operator=(Pipe&&) = delete;

  class PipeClosed {};

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

    bool AwaitReady() const { return !_Pipe._Data.has_value(); }
    void AwaitValue() {}

    AwaiterCustom<Producer, SingleAwaiter> Put(DataType&& data) && {
      assert(!_Pipe._IsClosed && !_Pipe._Data.has_value());
      _Pipe._Data.emplace(std::move(data));
      SingleAwaiter::Fire(_Pipe._AwaitReadContext);
      return AwaiterCustom<Producer, SingleAwaiter>(_Pipe._AwaitWriteContext, *this);
    }

    Coroutine<void> Close() && {
      assert(!_Pipe._IsClosed && !_Pipe._Data.has_value());
      _Pipe._IsClosed = true;
      SingleAwaiter::Fire(_Pipe._AwaitReadContext);
      co_return;
    }

  private:
    Pipe& _Pipe;
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

    bool AwaitReady() const { return _Pipe._IsClosed || _Pipe._Data.has_value(); }
    std::expected<DataType, PipeClosed> AwaitValue() {
      assert(_Pipe._IsClosed || _Pipe._Data.has_value());
      if (_Pipe._Data.has_value()) {
        auto ret = std::move(std::exchange(_Pipe._Data, std::nullopt).value());
        SingleAwaiter::Fire(_Pipe._AwaitWriteContext);
        return ret;
      } else if (_Pipe._IsClosed) {
        return std::unexpected<PipeClosed>{PipeClosed{}};
      } else {
        std::unreachable();
      }
    }

    AwaiterCustom<Consumer, SingleAwaiter> operator co_await() && {
      return AwaiterCustom<Consumer, SingleAwaiter>(_Pipe._AwaitReadContext, *this);
    }

  private:
    Pipe& _Pipe;
  };

  Producer GetProducer() { return Producer(*this); }
  Consumer GetConsumer() { return Consumer(*this); }

private:
  SingleAwaiter::ContextStorage _AwaitReadContext;
  SingleAwaiter::ContextStorage _AwaitWriteContext;
  bool _IsClosed = false;
  std::optional<DataType> _Data;
};

} // namespace Fiber
} // namespace Omni