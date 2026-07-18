#include "SymbolResolver.hpp"

#ifndef NDEBUG
#include <sstream>

#include "SymbolResolverDlAddr.hpp"
#include "SymbolResolverDwfl.hpp"
#include "SymbolResolverWin32.hpp"

#endif

namespace Omni::Fiber {

SymbolResolver::SymbolResolver() {
#ifndef NDEBUG
  if constexpr (SymbolResolverWin32::Enabled) {
    _Impls.push_back(std::make_unique<SymbolResolverWin32>());
  }
  if constexpr (SymbolResolverDwfl::Enabled) {
    _Impls.push_back(std::make_unique<SymbolResolverDwfl>());
  }
  if constexpr (SymbolResolverDlAddr::Enabled) {
    _Impls.push_back(std::make_unique<SymbolResolverDlAddr>());
  }
#endif
}

#ifndef NDEBUG
auto SymbolResolver::Resolve(void* address) -> std::string {
  if (address == nullptr) {
    return "nullptr";
  }

  for (auto& impl : _Impls) {
    if (auto res = impl->Resolve(address); res.has_value()) {
      std::ostringstream oss;
      oss << address << " (" << res.value() << ")";
      return oss.str();
    }
  }

  std::ostringstream oss;
  oss << address;
  return oss.str();
}
#endif

} // namespace Omni::Fiber
