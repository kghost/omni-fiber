#pragma once

#include <memory>
#include <optional>
#include <utility>

#include "FiberAwaitContext.hpp"
#include "FiberAwaitable.hpp"
#include "FiberAwaitableCustom.hpp"

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

    using AwaitResultType = void;
    bool AwaitReady() const { return !_Pipe._Data.has_value(); }
    void AwaitValue() {}

    void Put(DataType&& data) {
      assert(!_Pipe._Data.has_value());
      _Pipe._Data.emplace(PipeDataState::Data, std::move(data));
      if (auto readContext = _Pipe._AwaitReadContext.lock()) {
        readContext->Fire();
      }
    }

    void Close() {
      assert(!_Pipe._Data.has_value());
      _Pipe._Data.emplace(PipeDataState::End, std::nullopt);
      if (auto readContext = _Pipe._AwaitReadContext.lock()) {
        readContext->Fire();
      }
    }

    FiberAwaitableCustom<Producer> operator co_await() {
      return FiberAwaitableCustom<Producer>(FiberAwaitContext::Get(_Pipe._AwaitWriteContext), *this);
    }

  private:
    Pipe& _Pipe;
  };

  class Consumer {
  public:
    explicit Consumer(Pipe& pipe) : _Pipe(pipe) {}

    using AwaitResultType = PipeDataType;
    bool AwaitReady() const { return _Pipe._Data.has_value(); }
    PipeDataType AwaitValue() {
      assert(_Pipe._Data.has_value());
      PipeDataType ret = std::move(std::exchange(_Pipe._Data, std::nullopt).value());
      if (auto writeContext = _Pipe._AwaitWriteContext.lock()) {
        writeContext->Fire();
      }
      return ret;
    }

    FiberAwaitableCustom<Consumer> operator co_await() {
      return FiberAwaitableCustom<Consumer>(FiberAwaitContext::Get(_Pipe._AwaitReadContext), *this);
    }

  private:
    Pipe& _Pipe;
  };

  Producer GetProducer() { return Producer(*this); }
  Consumer GetConsumer() { return Consumer(*this); }

private:
  std::weak_ptr<FiberAwaitContext> _AwaitReadContext;
  std::weak_ptr<FiberAwaitContext> _AwaitWriteContext;
  std::optional<PipeDataType> _Data;
};

} // namespace Fiber
} // namespace Omni