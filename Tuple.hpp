#pragma once

#include <cstddef>
#include <utility>

namespace Omni::Fiber {

template <size_t Index, typename T> struct TupleLeaf {
  T value;
};

template <typename Seq, typename... Ts> struct TupleImpl;

template <size_t... Is, typename... Ts> struct TupleImpl<std::index_sequence<Is...>, Ts...> : TupleLeaf<Is, Ts>... {};

template <typename... Ts> using Tuple = TupleImpl<std::index_sequence_for<Ts...>, Ts...>;

template <size_t Index, typename T> auto Get(TupleLeaf<Index, T>& leaf) -> T& { return leaf.value; }

template <size_t Index, typename T> auto Get(const TupleLeaf<Index, T>& leaf) -> const T& { return leaf.value; }

template <typename Seq, typename... Ts> auto ApplyImpl(auto&& function, TupleImpl<Seq, Ts...>& tuple) {
  return [&]<size_t... Is>(std::index_sequence<Is...>) -> auto {
    return std::forward<decltype(function)>(function)(Get<Is>(tuple)...);
  }(Seq{});
}

template <typename Seq, typename... Ts> auto ApplyImpl(auto&& function, const TupleImpl<Seq, Ts...>& tuple) {
  return [&]<size_t... Is>(std::index_sequence<Is...>) -> auto {
    return std::forward<decltype(function)>(function)(Get<Is>(tuple)...);
  }(Seq{});
}

template <typename... Ts> auto Apply(auto&& function, Tuple<Ts...>& tuple) {
  return ApplyImpl(std::forward<decltype(function)>(function), tuple);
}

template <typename... Ts> auto Apply(auto&& function, const Tuple<Ts...>& tuple) {
  return ApplyImpl(std::forward<decltype(function)>(function), tuple);
}

} // namespace Omni::Fiber
