#pragma once

#include <string>

namespace Omni {
namespace Fiber {

#ifndef NDEBUG
std::string ResolveSymbol(void* address);
#endif

} // namespace Fiber
} // namespace Omni
