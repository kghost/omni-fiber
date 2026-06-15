#pragma once

#ifndef NDEBUG
#include <optional>
#include <string>

namespace Omni {
namespace Fiber {

std::optional<std::string> ResolveSymbolDwfl(void* address);

} // namespace Fiber
} // namespace Omni
#endif
