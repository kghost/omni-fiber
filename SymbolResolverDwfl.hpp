#pragma once

#ifndef NDEBUG
#include "SymbolResolver.hpp"
#include <optional>
#include <string>

#if __has_include(<dwarf.h>)
struct Dwfl;
#endif

namespace Omni::Fiber {

class SymbolResolverDwfl final : public SymbolResolver::Impl {
public:
  SymbolResolverDwfl(const SymbolResolverDwfl&) = delete;
  auto operator=(const SymbolResolverDwfl&) -> SymbolResolverDwfl& = delete;
  SymbolResolverDwfl(SymbolResolverDwfl&&) = delete;
  auto operator=(SymbolResolverDwfl&&) -> SymbolResolverDwfl& = delete;

#if __has_include(<dwarf.h>)
  static constexpr bool Enabled = true;

  explicit SymbolResolverDwfl();
  ~SymbolResolverDwfl() override;

  auto Resolve(void* address) -> std::optional<std::string> override;

private:
  Dwfl* _Dwfl = nullptr;
#else
  static constexpr bool Enabled = false;

  explicit SymbolResolverDwfl() = default;
  ~SymbolResolverDwfl() override = default;

  auto Resolve(void* /*address*/) -> std::optional<std::string> override { return std::nullopt; }
#endif
};

} // namespace Omni::Fiber

#endif
