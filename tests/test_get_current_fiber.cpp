#include <memory>
#include <string>

#include <boost/asio.hpp>
#include <gtest/gtest.h>

#include "Asio.hpp"
#include "Coroutine.hpp"
#include "Fiber.hpp"
#include "GetCurrentFiber.hpp"
#include "Manager.hpp"

using namespace Omni::Fiber;

namespace {

void RunEventLoop(boost::asio::io_context& io) {
  io.restart();
  io.run();
}

Coroutine<void> NestedLevel3(Fiber& expectedFiber, bool& executed) {
  Fiber& currentFiber = co_await GetCurrentFiber();
  EXPECT_EQ(&currentFiber, &expectedFiber);
  executed = true;
  co_return;
}

Coroutine<void> NestedLevel2(Fiber& expectedFiber, bool& executed) {
  co_await NestedLevel3(expectedFiber, executed);
  co_return;
}

Coroutine<void> NestedLevel1(Fiber& expectedFiber, bool& executed) {
  co_await NestedLevel2(expectedFiber, executed);
  co_return;
}

} // namespace

// Test Case 1: Retrieve Fiber in Root Fiber
TEST(GetCurrentFiberTest, RetrieveRootFiber) {
  boost::asio::io_context io;
  AsioExecutor executor(io.get_executor());
  Manager manager(executor);

  bool executed = false;

  std::shared_ptr<Fiber> root;
  root = manager.SpawnRoot("root", [&]() -> Coroutine<void> {
    Fiber& currentFiber = co_await GetCurrentFiber();
    // Since this is the root fiber, currentFiber should be the active root fiber
    EXPECT_EQ(&currentFiber, root.get());
    executed = true;
    co_return;
  });

  RunEventLoop(io);
  EXPECT_TRUE(executed);
}

// Test Case 2: Retrieve Fiber through deep nested coroutine calls
TEST(GetCurrentFiberTest, RetrieveInNestedCoroutines) {
  boost::asio::io_context io;
  AsioExecutor executor(io.get_executor());
  Manager manager(executor);

  bool executed = false;

  auto root = manager.SpawnRoot("root", [&]() -> Coroutine<void> {
    Fiber& currentFiber = co_await GetCurrentFiber();
    co_await NestedLevel1(currentFiber, executed);
    co_return;
  });

  RunEventLoop(io);
  EXPECT_TRUE(executed);
}

// Test Case 3: Multiple sibling fibers spawn and retrieve their own unique references
TEST(GetCurrentFiberTest, SiblingFibers) {
  boost::asio::io_context io;
  AsioExecutor executor(io.get_executor());
  Manager manager(executor);

  bool child1Executed = false;
  bool child2Executed = false;
  std::shared_ptr<Fiber> child1Ref;
  std::shared_ptr<Fiber> child2Ref;

  manager.SpawnRoot("root", [&]() -> Coroutine<void> {
    Fiber& current = co_await GetCurrentFiber();

    child1Ref = current.Spawn("child1", [&]() -> Coroutine<void> {
      Fiber& currentFiber = co_await GetCurrentFiber();
      EXPECT_EQ(&currentFiber, child1Ref.get());
      child1Executed = true;
      co_return;
    });

    child2Ref = current.Spawn("child2", [&]() -> Coroutine<void> {
      Fiber& currentFiber = co_await GetCurrentFiber();
      EXPECT_EQ(&currentFiber, child2Ref.get());
      child2Executed = true;
      co_return;
    });

    co_await current.Join(child1Ref);
    co_await current.Join(child2Ref);
    co_return;
  });

  RunEventLoop(io);
  EXPECT_TRUE(child1Executed);
  EXPECT_TRUE(child2Executed);
}
