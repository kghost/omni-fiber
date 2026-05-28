#include "SymbolResolver.hpp"

#ifndef NDEBUG
#include <cxxabi.h>
#include <dlfcn.h>
#include <dwarf.h>
#include <elfutils/libdwfl.h>
#include <sstream>
#include <unistd.h>

namespace Omni {
namespace Fiber {

namespace {

static Dwfl* GetDwfl() {
  static Dwfl* dwfl = []() -> Dwfl* {
    static const Dwfl_Callbacks callbacks = {
        .find_elf = dwfl_linux_proc_find_elf,
        .find_debuginfo = dwfl_standard_find_debuginfo,
        .section_address = dwfl_offline_section_address,
        .debuginfo_path = nullptr,
    };
    Dwfl* d = dwfl_begin(&callbacks);
    if (!d) {
      return nullptr;
    }
    if (dwfl_linux_proc_report(d, getpid()) != 0 || dwfl_report_end(d, nullptr, nullptr) != 0) {
      dwfl_end(d);
      return nullptr;
    }
    return d;
  }();
  return dwfl;
}

} // namespace

std::string ResolveSymbol(void* address) {
  if (!address) {
    return "nullptr";
  }
  std::ostringstream oss;
  oss << address;

  // 1. Attempt local DWARF symbol and line lookup using libdwfl
  Dwfl* dwfl = GetDwfl();
  if (dwfl) {
    Dwarf_Addr addr = reinterpret_cast<Dwarf_Addr>(address);
    Dwfl_Module* mod = dwfl_addrmodule(dwfl, addr);
    if (mod) {
      // Get symbol name
      GElf_Sym sym;
      GElf_Word shndx;
      const char* symName = dwfl_module_addrsym(mod, addr, &sym, &shndx);

      std::string resolvedName;
      if (symName) {
        int status = 0;
        char* demangled = abi::__cxa_demangle(symName, nullptr, nullptr, &status);
        if (status == 0 && demangled) {
          resolvedName = demangled;
          std::free(demangled);
        } else {
          resolvedName = symName;
        }
      }

      // Get DWARF source file and line info
      Dwfl_Line* line = dwfl_module_getsrc(mod, addr);
      std::string fileLine;
      if (line) {
        int lineNo = 0;
        const char* srcFile = dwfl_lineinfo(line, nullptr, &lineNo, nullptr, nullptr, nullptr);
        if (srcFile) {
          fileLine = std::string(srcFile) + ":" + std::to_string(lineNo);
        }
      }

      if (!resolvedName.empty() || !fileLine.empty()) {
        oss << " (";
        if (!resolvedName.empty()) {
          oss << resolvedName;
        } else {
          oss << "??";
        }
        if (!fileLine.empty()) {
          oss << " at " << fileLine;
        }
        oss << ")";
        return oss.str();
      }
    }
  }

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
  return oss.str();
}

} // namespace Fiber
} // namespace Omni
#endif
