#pragma once

#include <version>

#ifdef __cpp_lib_move_only_function
#include <functional>
namespace Omni::Fiber {
using std::move_only_function;
} // namespace Omni::Fiber
#else
#include "fu2/function2.hpp"
namespace Omni::Fiber {
template <typename Signature> using move_only_function = fu2::unique_function<Signature>;
} // namespace Omni::Fiber
#endif
