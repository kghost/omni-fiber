#pragma once

#include <concepts>
#include <optional>
#include <type_traits>

#include "AwaiterCustom.hpp"
#include "SharedAwaiter.hpp"

namespace Omni::Fiber {

template <typename DataType> class Event final {
public:
  explicit Event() = default;
  ~Event() = default;

  Event(Event&) = delete;
  auto operator=(Event&) -> Event& = delete;
  Event(Event&&) = delete;
  auto operator=(Event&&) -> Event& = delete;

  // ==== DataType == void
  template <typename U = DataType>
    requires(std::is_void_v<U>)
  [[nodiscard]] auto IsFired() const -> bool {
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
  [[nodiscard]] auto AwaitReady() const -> bool {
    return _Data;
  }

  template <typename U = DataType>
    requires(std::is_void_v<U>)
  void AwaitValue() {}

  // ==== DataType != void
  template <typename U = DataType>
    requires(!std::is_void_v<U>)
  [[nodiscard]] auto IsFired() const -> bool {
    return _Data.has_value();
  }

  template <typename U = DataType>
    requires(!std::is_void_v<U>)
  [[nodiscard]] auto Get() const -> DataType {
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
  [[nodiscard]] auto AwaitReady() const -> bool {
    return _Data.has_value();
  }

  template <typename U = DataType>
    requires(!std::is_void_v<U> && std::copyable<DataType>)
  auto AwaitValue() -> DataType {
    return _Data.value();
  }

  template <typename U = DataType>
    requires(!std::is_void_v<U> && !std::copyable<DataType> && std::movable<DataType>)
  auto AwaitValue() -> DataType {
    return std::move(_Data.value());
  }

  auto operator co_await() -> AwaiterCustom<Event, SharedAwaiter> {
    return AwaiterCustom<Event, SharedAwaiter>(_AwaitContext, *this);
  }

private:
  auto InitializeData() -> std::conditional_t<std::is_void_v<DataType>, bool, std::optional<DataType>> {
    if constexpr (std::is_void_v<DataType>) {
      return false;
    } else {
      return std::nullopt;
    }
  }

  SharedAwaiter::ContextStorage _AwaitContext;
  std::conditional_t<std::is_void_v<DataType>, bool, std::optional<DataType>> _Data = InitializeData();
};

} // namespace Omni::Fiber
