#pragma once

#include <optional>
#include <sys/epoll.h>
#include <type_traits>

#include "AwaitableCustom.hpp"
#include "SingleAwaitContext.hpp"
#include "SingleAwaitable.hpp"
#include "shared.h"

namespace Omni {
namespace Fiber {

template <typename DataType> class OneshotEvent final {
public:
  explicit OneshotEvent() = default;

  OneshotEvent(OneshotEvent&) = delete;
  OneshotEvent& operator=(OneshotEvent&) = delete;
  OneshotEvent(OneshotEvent&&) = delete;
  OneshotEvent& operator=(OneshotEvent&&) = delete;

  // ==== DataType == void
  template <typename U = DataType>
    requires(std::is_void_v<U>)
  bool IsFired() const {
    return _Data;
  }

  template <typename U = DataType>
    requires(std::is_void_v<U>)
  OMNIFIBER_API void Fire() {
    _Data = true;
    SingleAwaitable::Fire(_AwaitContext);
  }

  template <typename U = DataType>
    requires(std::is_void_v<U>)
  bool AwaitReady() const {
    return _Data;
  }

  template <typename U = DataType>
    requires(std::is_void_v<U>)
  void AwaitValue() {}

  // ==== DataType != void
  template <typename U = DataType>
    requires(!std::is_void_v<U>)
  bool IsFired() const {
    return _Data.has_value();
  }

  template <typename T, typename U = DataType>
    requires(!std::is_void_v<U>)
  OMNIFIBER_API void Fire(T&& data) {
    _Data.emplace(std::forward<T>(data));
    SingleAwaitable::Fire(_AwaitContext);
  }

  template <typename U = DataType>
    requires(!std::is_void_v<U>)
  bool AwaitReady() const {
    return _Data.has_value();
  }

  template <typename U = DataType>
    requires(!std::is_void_v<U>)
  DataType AwaitValue() {
    return std::move(_Data.value());
  }

  OMNIFIBER_API AwaitableCustom<OneshotEvent, SingleAwaitable> operator co_await() {
    return AwaitableCustom<OneshotEvent, SingleAwaitable>(_AwaitContext, *this);
  }

private:
  std::conditional_t<std::is_void_v<DataType>, bool, std::optional<DataType>> InitializeData() {
    if constexpr (std::is_void_v<DataType>) {
      return false;
    } else {
      return std::nullopt;
    }
  }

  SingleAwaitable::ContextStorage _AwaitContext;
  std::conditional_t<std::is_void_v<DataType>, bool, std::optional<DataType>> _Data = InitializeData();
};

} // namespace Fiber
} // namespace Omni