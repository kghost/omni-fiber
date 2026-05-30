#pragma once

#include <expected>
#include <optional>
#include <utility>

#include <boost/log/common.hpp>
#include <boost/log/trivial.hpp>

#include "AwaitableCustom.hpp"
#include "SharedAwaitContext.hpp"
#include "SharedAwaitable.hpp"
#include "SingleAwaitContext.hpp"
#include "SingleAwaitable.hpp"

namespace Omni {
namespace Fiber {

// A single consumer multiple producer pipe.
template <typename DataType> class SharedPipe final {
public:
  explicit SharedPipe() = default;

  SharedPipe(SharedPipe&) = delete;
  SharedPipe& operator=(SharedPipe&) = delete;
  SharedPipe(SharedPipe&&) = delete;
  SharedPipe& operator=(SharedPipe&&) = delete;

  class PipeClosed {};

  class Producer {
  public:
    explicit Producer(SharedPipe& pipe) : _Pipe(pipe) {}

    Producer(Producer&) = delete;
    Producer& operator=(Producer&) = delete;
    Producer(Producer&&) = delete;
    Producer& operator=(Producer&&) = delete;

    bool AwaitReady() const { return !_Pipe._Data.has_value(); }
    void AwaitValue() {}

    Coroutine<void> Put(DataType&& data) {
      co_await AwaitableCustom<Producer, SharedAwaitable>(_Pipe._AwaitWriteContext, *this);
      assert(!_Pipe._IsClosed && !_Pipe._Data.has_value());
      _Pipe._Data.emplace(std::move(data));
      SingleAwaitable::Fire(_Pipe._AwaitReadContext);
      co_await AwaitableCustom<Producer, SharedAwaitable>(_Pipe._AwaitWriteContext, *this);
    }

    Coroutine<void> Close() {
      co_await AwaitableCustom<Producer, SharedAwaitable>(_Pipe._AwaitWriteContext, *this);
      assert(!_Pipe._IsClosed && !_Pipe._Data.has_value());
      _Pipe._IsClosed = true;
      SingleAwaitable::Fire(_Pipe._AwaitReadContext);
    }

  private:
    SharedPipe& _Pipe;
  };

  class Consumer {
  public:
    explicit Consumer(SharedPipe& pipe) : _Pipe(pipe) {}
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
        SharedAwaitable::Fire(_Pipe._AwaitWriteContext);
        return ret;
      } else if (_Pipe._IsClosed) {
        return std::unexpected<PipeClosed>{PipeClosed{}};
      } else {
        std::unreachable();
      }
    }

    AwaitableCustom<Consumer, SingleAwaitable> operator co_await() {
      return AwaitableCustom<Consumer, SingleAwaitable>(_Pipe._AwaitReadContext, *this);
    }

  private:
    SharedPipe& _Pipe;
  };

  Producer GetProducer() { return Producer(*this); }
  Consumer GetConsumer() { return Consumer(*this); }

private:
  SingleAwaitable::ContextStorage _AwaitReadContext;
  SharedAwaitable::ContextStorage _AwaitWriteContext;
  bool _IsClosed = false;
  std::optional<DataType> _Data;
};

} // namespace Fiber
} // namespace Omni