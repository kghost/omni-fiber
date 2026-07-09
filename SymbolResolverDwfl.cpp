#include "SymbolResolverDwfl.hpp"

#include <format>
#include <optional>

#ifndef NDEBUG
#if __has_include(<dwarf.h>)
#include <cxxabi.h>
#include <dlfcn.h>
#include <dwarf.h>
#include <elfutils/libdw.h>
#include <elfutils/libdwfl.h>
#include <unistd.h>

namespace Omni::Fiber {

namespace {

auto GetDwfl() -> Dwfl* {
  static Dwfl* dwfl = []() -> Dwfl* {
    static const Dwfl_Callbacks callbacks = {
        .find_elf = dwfl_linux_proc_find_elf,
        .find_debuginfo = dwfl_standard_find_debuginfo,
        .section_address = dwfl_offline_section_address,
        .debuginfo_path = nullptr,
    };
    Dwfl* dwfl = dwfl_begin(&callbacks);
    if (!dwfl) {
      return nullptr;
    }
    if (dwfl_linux_proc_report(dwfl, getpid()) != 0 || dwfl_report_end(dwfl, nullptr, nullptr) != 0) {
      dwfl_end(dwfl);
      return nullptr;
    }
    return dwfl;
  }();
  return dwfl;
}

} // namespace

auto ResolveSymbolDwfl(void* address) -> std::optional<std::string> {
  // 1. Attempt local DWARF symbol and line lookup using libdwfl
  Dwfl* dwfl = GetDwfl();
  if (dwfl != nullptr) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    auto addr = reinterpret_cast<Dwarf_Addr>(address);
    Dwfl_Module* mod = dwfl_addrmodule(dwfl, addr);
    if (mod != nullptr) {
      // Get symbol name
      GElf_Sym sym;
      GElf_Word shndx = 0;
      const char* symName = dwfl_module_addrsym(mod, addr, &sym, &shndx);

      std::string resolvedName;
      if (symName != nullptr) {
        int status = 0;
        char* demangled = abi::__cxa_demangle(symName, nullptr, nullptr, &status);
        if (status == 0 && (demangled != nullptr)) {
          resolvedName = demangled;
          std::free(demangled); // NOLINT(cppcoreguidelines-owning-memory,cppcoreguidelines-no-malloc)
        } else {
          resolvedName = symName;
        }
      } else {
        // Fallback to DWARF DIE scope lookup if symbol table lookup fails
        Dwarf_Addr bias = 0;
        Dwarf_Die* cudie = dwfl_module_addrdie(mod, addr, &bias);
        if (cudie != nullptr) {
          Dwarf_Die* scopes = nullptr;
          int nscopes = dwarf_getscopes(cudie, addr - bias, &scopes);
          if (nscopes > 0) {
            std::span<Dwarf_Die>(scopes, nscopes);
            for (auto& scope : std::span<Dwarf_Die>(scopes, nscopes)) {
              int tag = dwarf_tag(&scope);
              if (tag == DW_TAG_subprogram || tag == DW_TAG_inlined_subroutine) {
                Dwarf_Attribute attr;
                const char* name = nullptr;
                if ((dwarf_attr_integrate(&scope, DW_AT_linkage_name, &attr) != nullptr) ||
                    (dwarf_attr_integrate(&scope, DW_AT_MIPS_linkage_name, &attr) != nullptr) ||
                    (dwarf_attr_integrate(&scope, DW_AT_name, &attr) != nullptr)) {
                  name = dwarf_formstring(&attr);
                }
                if (name != nullptr) {
                  int status = 0;
                  char* demangled = abi::__cxa_demangle(name, nullptr, nullptr, &status);
                  if (status == 0 && (demangled != nullptr)) {
                    resolvedName = demangled;
                    std::free(demangled); // NOLINT(cppcoreguidelines-owning-memory,cppcoreguidelines-no-malloc)
                  } else {
                    resolvedName = name;
                  }
                  break;
                }
              }
            }
            std::free(scopes); // NOLINT(cppcoreguidelines-owning-memory,cppcoreguidelines-no-malloc)
          }
        }
      }

      // Get DWARF source file and line info
      Dwfl_Line* line = dwfl_module_getsrc(mod, addr);
      std::string fileLine;
      if (line != nullptr) {
        int lineNo = 0;
        const char* srcFile = dwfl_lineinfo(line, nullptr, &lineNo, nullptr, nullptr, nullptr);
        if (srcFile != nullptr) {
          fileLine = std::string(srcFile) + ":" + std::to_string(lineNo);
        }
      }

      if (!resolvedName.empty()) {
        if (!fileLine.empty()) {
          return std::format("{} @ {}", resolvedName, fileLine);
        } else {
          return resolvedName;
        }
      } else {
        if (!fileLine.empty()) {
          return std::format("?? @ {}", fileLine);
        } else {
          return std::nullopt;
        }
      }
    }
  }

  return std::nullopt;
}

} // namespace Omni::Fiber

#else
namespace Omni {
namespace Fiber {

std::optional<std::string> ResolveSymbolDwfl(void*) { return std::nullopt; }

} // namespace Fiber
} // namespace Omni

#endif
#endif