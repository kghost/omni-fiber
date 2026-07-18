#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace Omni::Fiber {

class SymbolResolver {
public:
  explicit SymbolResolver();
  ~SymbolResolver() = default;

  SymbolResolver(const SymbolResolver&) = delete;
  auto operator=(const SymbolResolver&) -> SymbolResolver& = delete;
  SymbolResolver(SymbolResolver&&) = delete;
  auto operator=(SymbolResolver&&) -> SymbolResolver& = delete;

#ifndef NDEBUG
  auto Resolve(void* address) -> std::string;

  class Impl {
  public:
    explicit Impl() = default;
    virtual ~Impl() = default;

    Impl(const Impl&) = delete;
    Impl(Impl&&) = delete;
    auto operator=(const Impl&) -> Impl& = delete;
    auto operator=(Impl&&) -> Impl& = delete;

    virtual auto Resolve(void* address) -> std::optional<std::string> = 0;
  };

private:
  std::vector<std::unique_ptr<Impl>> _Impls;
#endif
};

} // namespace Omni::Fiber
