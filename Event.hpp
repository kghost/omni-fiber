#pragma once

#include <concepts>
#include <optional>
#include <type_traits>

#include "AwaiterCustom.hpp"
#include "SharedAwaiter.hpp"

namespace Omni {
namespace Fiber {

template <typename DataType> class Event final {
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
  void Fire() {
    _Data = true;
    SharedAwaiter::Fire(_AwaitContext);
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
  void Fire(T&& data) {
    _Data.emplace(std::forward<T>(data));
    SharedAwaiter::Fire(_AwaitContext);
  }

  template <typename U = DataType>
    requires(!std::is_void_v<U>)
  bool AwaitReady() const {
    return _Data.has_value();
  }

  template <typename U = DataType>
    requires(!std::is_void_v<U> && std::copyable<DataType>)
  DataType AwaitValue() {
    return _Data.value();
  }

  template <typename U = DataType>
    requires(!std::is_void_v<U> && !std::copyable<DataType> && std::movable<DataType>)
  DataType AwaitValue() {
    return std::move(_Data.value());
  }

  AwaiterCustom<Event, SharedAwaiter> operator co_await() {
    return AwaiterCustom<Event, SharedAwaiter>(_AwaitContext, *this);
  }

private:
  std::conditional_t<std::is_void_v<DataType>, bool, std::optional<DataType>> InitializeData() {
    if constexpr (std::is_void_v<DataType>) {
      return false;
    } else {
      return std::nullopt;
    }
  }

  SharedAwaiter::ContextStorage _AwaitContext;
  std::conditional_t<std::is_void_v<DataType>, bool, std::optional<DataType>> _Data = InitializeData();
};

} // namespace Fiber
} // namespace Omni