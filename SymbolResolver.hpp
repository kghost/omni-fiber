#pragma once

#include <string>

namespace Omni::Fiber {
#ifndef NDEBUG
auto ResolveSymbol(void* address) -> std::string;
#endif
} // namespace Omni::Fiber
