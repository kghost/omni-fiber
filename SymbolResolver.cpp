#include "SymbolResolver.hpp"

#ifndef NDEBUG
#if __has_include(<dlfcn.h>)
#include <cxxabi.h>
#include <dlfcn.h>
#endif
#include <sstream>

#include "SymbolResolverDwfl.hpp"

namespace Omni {
namespace Fiber {

std::string ResolveSymbol(void* address) {
  if (!address) {
    return "nullptr";
  }
  std::ostringstream oss;
  oss << address;

  if (auto str = ResolveSymbolDwfl(address); str.has_value()) {
    oss << " (" << str.value() << ")";
    return oss.str();
  }

#if __has_include(<dlfcn.h>)
  // 2. Fallback to dladdr if DWARF resolution is not available or failed
  Dl_info info;
  if (dladdr(address, &info) && info.dli_sname) {
    int status = 0;
    char* demangled = abi::__cxa_demangle(info.dli_sname, nullptr, nullptr, &status);
    if (status == 0 && demangled) {
      oss << " (" << demangled << ")";
      std::free(demangled);
    } else {
      oss << " (" << info.dli_sname << ")";
    }
  }
#endif

  return oss.str();
}

} // namespace Fiber
} // namespace Omni
#endif
