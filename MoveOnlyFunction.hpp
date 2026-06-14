#pragma once

#include <version>

#if defined(__cpp_lib_move_only_function)
#include <functional>
namespace Omni {
namespace Fiber {
using std::move_only_function;
}
}
#else
#include "fu2/function2.hpp"
namespace Omni {
namespace Fiber {
template <typename Signature>
using move_only_function = fu2::unique_function<Signature>;
}
}
#endif
