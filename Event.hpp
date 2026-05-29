#pragma once

#include <optional>
#include <sys/epoll.h>
#include <type_traits>

#include "AwaitableCustom.hpp"
#include "SharedAwaitable.hpp"

#include "shared.h"

namespace Omni {
namespace Fiber {

template <typename DataType = void> class Event final {
public:
  explicit Event() = default;

  Event(Event&) = delete;
  Event& operator=(Event&) = delete;
  Event(Event&&) = delete;
  Event& operator=(Event&&) = delete;

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
    SharedAwaitable::Fire(_AwaitContext);
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

  template <typename U = DataType>
    requires(!std::is_void_v<U>)
  DataType Get() const {
    return _Data.value();
  }

  template <typename T, typename U = DataType>
    requires(!std::is_void_v<U>)
  OMNIFIBER_API void Fire(T&& data) {
    _Data.emplace(std::forward<T>(data));
    SharedAwaitable::Fire(_AwaitContext);
  }

  template <typename U = DataType>
    requires(!std::is_void_v<U>)
  bool AwaitReady() const {
    return _Data.has_value();
  }

  template <typename U = DataType>
    requires(!std::is_void_v<U>)
  DataType AwaitValue() {
    return _Data.value();
  }

  OMNIFIBER_API AwaitableCustom<Event, SharedAwaitable> operator co_await() {
    return AwaitableCustom<Event, SharedAwaitable>(_AwaitContext, *this);
  }

private:
  std::conditional_t<std::is_void_v<DataType>, bool, std::optional<DataType>> InitializeData() {
    if constexpr (std::is_void_v<DataType>) {
      return false;
    } else {
      return std::nullopt;
    }
  }

  SharedAwaitable::ContextStorage _AwaitContext;
  std::conditional_t<std::is_void_v<DataType>, bool, std::optional<DataType>> _Data = InitializeData();
};

} // namespace Fiber
} // namespace Omni