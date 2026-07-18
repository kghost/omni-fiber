#include "SymbolResolverDlAddr.hpp"

#ifndef NDEBUG
#if __has_include(<dlfcn.h>)
#include <cxxabi.h>
#include <dlfcn.h>

namespace Omni::Fiber {

auto SymbolResolverDlAddr::Resolve(void* address) -> std::optional<std::string> {
  Dl_info info;
  if ((dladdr(address, &info) != 0) && (info.dli_sname != nullptr)) {
    int status = 0;
    char* demangled = abi::__cxa_demangle(info.dli_sname, nullptr, nullptr, &status);
    std::string result;
    if (status == 0 && (demangled != nullptr)) {
      result = demangled;
      std::free(demangled); // NOLINT(cppcoreguidelines-owning-memory,cppcoreguidelines-no-malloc)
    } else {
      result = info.dli_sname;
    }
    return result;
  }
  return std::nullopt;
}

} // namespace Omni::Fiber
#endif
#endif
