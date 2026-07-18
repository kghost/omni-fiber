#include "SymbolResolverWin32.hpp"

#ifndef NDEBUG
#if defined(_WIN32)
#include <windows.h>

#include <dbghelp.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <format>

namespace Omni::Fiber {

SymbolResolverWin32::SymbolResolverWin32() = default;

SymbolResolverWin32::~SymbolResolverWin32() {
  if (_DbgHelpInitialized) {
    SymCleanup(GetCurrentProcess());
  }
}

auto SymbolResolverWin32::Resolve(void* address) -> std::optional<std::string> {
  if (address == nullptr) {
    return std::nullopt;
  }

  // Follow MSVC ILT jump if present
#if defined(_M_IX86) || defined(_M_X64)
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
  auto* code = reinterpret_cast<unsigned char*>(address);
  constexpr unsigned char JumpOpcode = 0xE9;
  // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  if (code != nullptr && *code == JumpOpcode) {
    int32_t offset = 0;
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    std::memcpy(&offset, code + 1, sizeof(int32_t));
    constexpr int JumpInstructionLength = 5;
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    address = code + JumpInstructionLength + offset;
  }
#endif

  if (!_DbgHelpInitialized) {
    HANDLE process = GetCurrentProcess();
    SymSetOptions(SymGetOptions() | SYMOPT_LOAD_LINES | SYMOPT_UNDNAME);
    if (SymInitialize(process, nullptr, TRUE) != 0) {
      _DbgHelpInitialized = true;
    }
  }

  if (!_DbgHelpInitialized) {
    return std::nullopt;
  }

  HANDLE process = GetCurrentProcess();
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
  auto addr = reinterpret_cast<DWORD64>(address);

  // Buffer for symbol info
  constexpr size_t BufferSize = sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(char);
  alignas(SYMBOL_INFO) std::array<char, BufferSize> buffer{};
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
  auto* symbol = reinterpret_cast<SYMBOL_INFO*>(buffer.data());
  symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
  symbol->MaxNameLen = MAX_SYM_NAME;

  DWORD64 displacement = 0;
  std::string resolvedName;
  if (SymFromAddr(process, addr, &displacement, symbol) != 0) {
    resolvedName = &symbol->Name[0];
  }

  // Get file name and line number
  DWORD displacementLine = 0;
  IMAGEHLP_LINE64 line = {.SizeOfStruct = 0};
  line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
  std::string fileLine;
  if (SymGetLineFromAddr64(process, addr, &displacementLine, &line) != 0) {
    if (line.FileName != nullptr) {
      fileLine = std::string(line.FileName) + ":" + std::to_string(line.LineNumber);
    }
  }

  if (!resolvedName.empty()) {
    if (!fileLine.empty()) {
      return std::format("{} @ {}", resolvedName, fileLine);
    }
    return resolvedName;
  } else if (!fileLine.empty()) {
    return std::format("?? @ {}", fileLine);
  }

  return std::nullopt;
}

} // namespace Omni::Fiber

#endif
#endif
