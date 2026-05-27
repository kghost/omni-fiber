#pragma once

#include <optional>
#include <utility>

#include "AwaitableCustom.hpp"
#include "SingleAwaitContext.hpp"
#include "SingleAwaitable.hpp"

namespace Omni {
namespace Fiber {

template <typename DataType> class Pipe final {
public:
  explicit Pipe() = default;

  Pipe(Pipe&) = delete;
  Pipe& operator=(Pipe&) = delete;
  Pipe(Pipe&&) = delete;
  Pipe& operator=(Pipe&&) = delete;

  enum class PipeDataState {
    Data,
    End,
  };

  using PipeDataType = std::tuple<PipeDataState, std::optional<DataType>>;

  class Producer {
  public:
    explicit Producer(Pipe& pipe) : _Pipe(pipe) {}

    bool AwaitReady() const { return !_Pipe._Data.has_value(); }
    void AwaitValue() {}

    AwaitableCustom<Producer, SingleAwaitable> Put(DataType&& data) {
      assert(!_Pipe._Data.has_value());
      _Pipe._Data.emplace(PipeDataState::Data, std::move(data));
      SingleAwaitable::Fire(_Pipe._AwaitReadContext);
      return AwaitableCustom<Producer, SingleAwaitable>(_Pipe._AwaitWriteContext, *this);
    }

    AwaitableCustom<Producer, SingleAwaitable> Close() {
      assert(!_Pipe._Data.has_value());
      _Pipe._Data.emplace(PipeDataState::End, std::nullopt);
      SingleAwaitable::Fire(_Pipe._AwaitReadContext);
      return AwaitableCustom<Producer, SingleAwaitable>(_Pipe._AwaitWriteContext, *this);
    }

  private:
    Pipe& _Pipe;
  };

  class Consumer {
  public:
    explicit Consumer(Pipe& pipe) : _Pipe(pipe) {}

    bool AwaitReady() const { return _Pipe._Data.has_value(); }
    PipeDataType AwaitValue() {
      assert(_Pipe._Data.has_value());
      PipeDataType ret = std::move(std::exchange(_Pipe._Data, std::nullopt).value());
      SingleAwaitable::Fire(_Pipe._AwaitWriteContext);
      return ret;
    }

    AwaitableCustom<Consumer, SingleAwaitable> operator co_await() {
      return AwaitableCustom<Consumer, SingleAwaitable>(_Pipe._AwaitReadContext, *this);
    }

  private:
    Pipe& _Pipe;
  };

  Producer GetProducer() { return Producer(*this); }
  Consumer GetConsumer() { return Consumer(*this); }

private:
  SingleAwaitable::ContextStorage _AwaitReadContext;
  SingleAwaitable::ContextStorage _AwaitWriteContext;
  std::optional<PipeDataType> _Data;
};

} // namespace Fiber
} // namespace Omni