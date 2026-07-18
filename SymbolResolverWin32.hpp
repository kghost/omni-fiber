#pragma once

#ifndef NDEBUG
#include "SymbolResolver.hpp"

namespace Omni::Fiber {

class SymbolResolverWin32 final : public SymbolResolver::Impl {
public:
#if defined(_WIN32)
  explicit SymbolResolverWin32();
  ~SymbolResolverWin32() override;
#else
  explicit SymbolResolverWin32() = default;
  ~SymbolResolverWin32() override = default;
#endif

  SymbolResolverWin32(const SymbolResolverWin32&) = delete;
  auto operator=(const SymbolResolverWin32&) -> SymbolResolverWin32& = delete;
  SymbolResolverWin32(SymbolResolverWin32&&) = delete;
  auto operator=(SymbolResolverWin32&&) -> SymbolResolverWin32& = delete;

#if defined(_WIN32)
  static constexpr bool Enabled = true;
  auto Resolve(void* address) -> std::optional<std::string> override;
#else
  static constexpr bool Enabled = false;
  auto Resolve(void* /*address*/) -> std::optional<std::string> override { return std::nullopt; }
#endif

private:
  bool _DbgHelpInitialized = false;
};

} // namespace Omni::Fiber

#endif
