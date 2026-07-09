#pragma once

#ifndef NDEBUG
#include <optional>
#include <string>

namespace Omni::Fiber {
auto ResolveSymbolDwfl(void* address) -> std::optional<std::string>;
} // namespace Omni::Fiber

#endif
