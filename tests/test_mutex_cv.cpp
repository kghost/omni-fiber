#include <string>
#include <vector>

#include <boost/asio.hpp>
#include <gtest/gtest.h>

#include "Asio.hpp"
#include "ConditionalVariable.hpp"
#include "Coroutine.hpp"
#include "Event.hpp"
#include "Fiber.hpp"
#include "GetCurrentOmniFiber.hpp"
#include "Manager.hpp"
#include "Mutex.hpp"
#include "Select.hpp"
#include "SelectPair.hpp"

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
    co_await mutex.Lock();
    EXPECT_TRUE(mutex.IsLocked());
    executed = true;
    mutex.Unlock();
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
      co_await mutex.Lock();
      log.push_back("fiber1_enter");
      // yield to let fiber2 run if possible
      boost::asio::steady_timer timer(io, std::chrono::milliseconds(10));
      co_await timer.async_wait(AsioUseFiber);
      log.push_back("fiber1_exit");
      mutex.Unlock();
      co_return;
    });

    auto child2 = current.Spawn("fiber2", [&]() -> Coroutine<void> {
      co_await mutex.Lock();
      log.push_back("fiber2_enter");
      log.push_back("fiber2_exit");
      mutex.Unlock();
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

TEST(ConditionalVariableTest, BasicWaitNotify) {
  boost::asio::io_context io;
  AsioExecutor executor(io.get_executor());
  Manager manager(executor);

  Mutex mutex;
  ConditionalVariable cv;
  bool ready = false;
  std::vector<std::string> log;

  manager.SpawnRoot("root", [&]() -> Coroutine<void> {
    Fiber& current = co_await GetCurrentOmniFiber();

    auto waiter = current.Spawn("waiter", [&]() -> Coroutine<void> {
      co_await mutex.Lock();
      log.push_back("waiter_locked");
      co_await cv.Wait(mutex, [&]() { return ready; });
      log.push_back("waiter_notified");
      mutex.Unlock();
      co_return;
    });

    auto notifier = current.Spawn("notifier", [&]() -> Coroutine<void> {
      boost::asio::steady_timer timer(io, std::chrono::milliseconds(10));
      co_await timer.async_wait(AsioUseFiber);

      co_await mutex.Lock();
      log.push_back("notifier_locked");
      ready = true;
      cv.NotifyOne();
      mutex.Unlock();
      log.push_back("notifier_unlocked");
      co_return;
    });

    co_await current.Join(waiter);
    co_await current.Join(notifier);
    co_return;
  });

  RunEventLoop(io);

  ASSERT_EQ(log.size(), 4);
  EXPECT_EQ(log[0], "waiter_locked");
  EXPECT_EQ(log[1], "notifier_locked");
  EXPECT_EQ(log[2], "notifier_unlocked");
  EXPECT_EQ(log[3], "waiter_notified");
}

TEST(ConditionalVariableTest, NotifyAll) {
  boost::asio::io_context io;
  AsioExecutor executor(io.get_executor());
  Manager manager(executor);

  Mutex mutex;
  ConditionalVariable cv;
  bool ready = false;
  int wokenCount = 0;

  manager.SpawnRoot("root", [&]() -> Coroutine<void> {
    Fiber& current = co_await GetCurrentOmniFiber();

    auto waiter1 = current.Spawn("waiter1", [&]() -> Coroutine<void> {
      co_await mutex.Lock();
      co_await cv.Wait(mutex, [&]() { return ready; });
      wokenCount++;
      mutex.Unlock();
      co_return;
    });

    auto waiter2 = current.Spawn("waiter2", [&]() -> Coroutine<void> {
      co_await mutex.Lock();
      co_await cv.Wait(mutex, [&]() { return ready; });
      wokenCount++;
      mutex.Unlock();
      co_return;
    });

    auto notifier = current.Spawn("notifier", [&]() -> Coroutine<void> {
      boost::asio::steady_timer timer(io, std::chrono::milliseconds(10));
      co_await timer.async_wait(AsioUseFiber);

      co_await mutex.Lock();
      ready = true;
      cv.NotifyAll();
      mutex.Unlock();
      co_return;
    });

    co_await current.Join(waiter1);
    co_await current.Join(waiter2);
    co_await current.Join(notifier);
    co_return;
  });

  RunEventLoop(io);

  EXPECT_EQ(wokenCount, 2);
}

TEST(SelectTest, MutexCancellation) {
  boost::asio::io_context io;
  AsioExecutor executor(io.get_executor());
  Manager manager(executor);

  Mutex mutex;
  Event<void> event;
  bool eventFired = false;
  bool mutexLocked = false;

  // Lock mutex beforehand so `mutex.Lock()` would suspend
  manager.SpawnRoot("root", [&]() -> Coroutine<void> {
    Fiber& current = co_await GetCurrentOmniFiber();

    co_await mutex.Lock();

    auto selector = current.Spawn("selector", [&]() -> Coroutine<void> {
      co_await Select(SelectPair(mutex, [&]() { mutexLocked = true; }),
                      SelectPair(event, [&]() { eventFired = true; }));
      co_return;
    });

    auto fireFiber = current.Spawn("fireFiber", [&]() -> Coroutine<void> {
      boost::asio::steady_timer timer(io, std::chrono::milliseconds(10));
      co_await timer.async_wait(AsioUseFiber);
      event.Fire();
      co_return;
    });

    co_await current.Join(selector);
    co_await current.Join(fireFiber);

    mutex.Unlock();
    co_return;
  });

  RunEventLoop(io);

  EXPECT_TRUE(eventFired);
  EXPECT_FALSE(mutexLocked);
}
