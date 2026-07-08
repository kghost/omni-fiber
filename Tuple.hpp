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

template <size_t Index, typename T> T& Get(TupleLeaf<Index, T>& leaf) { return leaf.value; }

template <size_t Index, typename T> const T& Get(const TupleLeaf<Index, T>& leaf) { return leaf.value; }

template <typename F, typename Seq, typename... Ts> auto ApplyImpl(F&& f, TupleImpl<Seq, Ts...>& t) {
  return [&]<size_t... Is>(std::index_sequence<Is...>) { return std::forward<F>(f)(Get<Is>(t)...); }(Seq{});
}

template <typename F, typename Seq, typename... Ts> auto ApplyImpl(F&& f, const TupleImpl<Seq, Ts...>& t) {
  return [&]<size_t... Is>(std::index_sequence<Is...>) { return std::forward<F>(f)(Get<Is>(t)...); }(Seq{});
}

template <typename F, typename... Ts> auto Apply(F&& f, Tuple<Ts...>& t) { return ApplyImpl(std::forward<F>(f), t); }

template <typename F, typename... Ts> auto Apply(F&& f, const Tuple<Ts...>& t) {
  return ApplyImpl(std::forward<F>(f), t);
}

} // namespace Omni::Fiber
