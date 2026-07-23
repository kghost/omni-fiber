#include <string>
#include <vector>

#include <boost/asio.hpp>
#include <gtest/gtest.h>

#include "Asio.hpp"
#include "Coroutine.hpp"
#include "Fiber.hpp"
#include "GetCurrentOmniFiber.hpp"
#include "Manager.hpp"
#include "Mutex.hpp"

using namespace Omni::Fiber;

namespace {

void RunEventLoop(boost::asio::io_context& io) {
  io.restart();
  io.run();
}

} // namespace

TEST(MutexTest, BasicLockUnlock) {
  boost::asio::io_context io;
  AsioExecutor executor(io.get_executor());
  Manager manager(executor);

  Mutex mutex;
  EXPECT_FALSE(mutex.IsLocked());

  bool executed = false;

  manager.SpawnRoot("root", [&]() -> Coroutine<void> {
    {
      auto guard = co_await mutex.Wait();
      EXPECT_TRUE(mutex.IsLocked());
      executed = true;
    }
    EXPECT_FALSE(mutex.IsLocked());
    co_return;
  });

  RunEventLoop(io);
  EXPECT_TRUE(executed);
}

TEST(MutexTest, MutualExclusionAndFIFO) {
  boost::asio::io_context io;
  AsioExecutor executor(io.get_executor());
  Manager manager(executor);

  Mutex mutex;
  std::vector<std::string> log;

  manager.SpawnRoot("root", [&]() -> Coroutine<void> {
    Fiber& current = co_await GetCurrentOmniFiber();

    auto child1 = current.Spawn("fiber1", [&]() -> Coroutine<void> {
      auto guard = co_await mutex.Wait();
      log.push_back("fiber1_enter");
      // yield to let fiber2 run if possible
      boost::asio::steady_timer timer(io, std::chrono::milliseconds(10));
      co_await timer.async_wait(AsioUseFiber);
      log.push_back("fiber1_exit");
      co_return;
    });

    auto child2 = current.Spawn("fiber2", [&]() -> Coroutine<void> {
      auto guard = co_await mutex.Wait();
      log.push_back("fiber2_enter");
      log.push_back("fiber2_exit");
      co_return;
    });

    co_await current.Join(child1);
    co_await current.Join(child2);
    co_return;
  });

  RunEventLoop(io);

  ASSERT_EQ(log.size(), 4);
  EXPECT_EQ(log[0], "fiber1_enter");
  EXPECT_EQ(log[1], "fiber1_exit");
  EXPECT_EQ(log[2], "fiber2_enter");
  EXPECT_EQ(log[3], "fiber2_exit");
}
