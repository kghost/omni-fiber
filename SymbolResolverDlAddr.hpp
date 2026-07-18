#pragma once

#ifndef NDEBUG
#include "SymbolResolver.hpp"

namespace Omni::Fiber {

class SymbolResolverDlAddr final : public SymbolResolver::Impl {
public:
  explicit SymbolResolverDlAddr() = default;
  ~SymbolResolverDlAddr() override = default;

  SymbolResolverDlAddr(const SymbolResolverDlAddr&) = delete;
  auto operator=(const SymbolResolverDlAddr&) -> SymbolResolverDlAddr& = delete;
  SymbolResolverDlAddr(SymbolResolverDlAddr&&) = delete;
  auto operator=(SymbolResolverDlAddr&&) -> SymbolResolverDlAddr& = delete;

#if __has_include(<dlfcn.h>)
  static constexpr bool Enabled = true;
  auto Resolve(void* address) -> std::optional<std::string> override;
#else
  static constexpr bool Enabled = false;
  auto Resolve(void* /*address*/) -> std::optional<std::string> override { return std::nullopt; }
#endif
};

} // namespace Omni::Fiber

#endif
