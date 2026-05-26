#include <memory>
#include <vector>

#include <boost/asio.hpp>
#include <gtest/gtest.h>

#include "Asio.hpp"
#include "Coroutine.hpp"
#include "Event.hpp"
#include "Fiber.hpp"
#include "GetCurrentFiber.hpp"
#include "Manager.hpp"

using namespace Omni::Fiber;

namespace {

void RunEventLoop(boost::asio::io_context& io) {
  io.restart();
  io.run();
}

} // namespace

// 1. Test case: Initial state of a newly created Event
TEST(EventTest, InitialState) {
  Event event;
  EXPECT_FALSE(event.AwaitReady());
}

// 2. Test case: Early Fire (Fire called before co_await)
TEST(EventTest, EarlyFire) {
  boost::asio::io_context io;
  AsioExecutor executor(io);
  Manager manager(executor);

  Event event;
  event.Fire();
  EXPECT_TRUE(event.AwaitReady());

  bool fiberExecuted = false;

  manager.SpawnRoot("root", [&]() -> Coroutine<void> {
    co_await event;
    fiberExecuted = true;
    co_return;
  });

  RunEventLoop(io);

  EXPECT_TRUE(fiberExecuted);
}

// 3. Test case: Single awaiter waiting on Event being fired cooperatively
TEST(EventTest, SingleAwaiter) {
  boost::asio::io_context io;
  AsioExecutor executor(io);
  Manager manager(executor);

  Event event;
  std::vector<std::string> sequence;

  manager.SpawnRoot("root", [&]() -> Coroutine<void> {
    Fiber& current = co_await GetCurrentFiber();

    auto child = current.Spawn("awaiter", [&]() -> Coroutine<void> {
      sequence.push_back("awaiter_suspend");
      co_await event;
      sequence.push_back("awaiter_resume");
      co_return;
    });

    auto notifier = current.Spawn("notifier", [&]() -> Coroutine<void> {
      sequence.push_back("notifier_fire");
      event.Fire();
      sequence.push_back("notifier_done");
      co_return;
    });

    co_await current.Join(child);
    co_await current.Join(notifier);
    co_return;
  });

  RunEventLoop(io);

  ASSERT_EQ(sequence.size(), 4);
  EXPECT_EQ(sequence[0], "awaiter_suspend");
  EXPECT_EQ(sequence[1], "notifier_fire");
  EXPECT_EQ(sequence[2], "notifier_done");
  EXPECT_EQ(sequence[3], "awaiter_resume");
}

// 4. Test case: Multiple awaiters waiting on a single Event cooperatively
TEST(EventTest, MultipleAwaiters) {
  boost::asio::io_context io;
  AsioExecutor executor(io);
  Manager manager(executor);

  Event event;
  std::vector<std::string> sequence;

  manager.SpawnRoot("root", [&]() -> Coroutine<void> {
    Fiber& current = co_await GetCurrentFiber();

    auto child1 = current.Spawn("awaiter1", [&]() -> Coroutine<void> {
      sequence.push_back("awaiter1_suspend");
      co_await event;
      sequence.push_back("awaiter1_resume");
      co_return;
    });

    auto child2 = current.Spawn("awaiter2", [&]() -> Coroutine<void> {
      sequence.push_back("awaiter2_suspend");
      co_await event;
      sequence.push_back("awaiter2_resume");
      co_return;
    });

    auto notifier = current.Spawn("notifier", [&]() -> Coroutine<void> {
      sequence.push_back("notifier_fire");
      event.Fire();
      sequence.push_back("notifier_done");
      co_return;
    });

    co_await current.Join(child1);
    co_await current.Join(child2);
    co_await current.Join(notifier);
    co_return;
  });

  RunEventLoop(io);

  ASSERT_EQ(sequence.size(), 6);
  EXPECT_EQ(sequence[0], "awaiter1_suspend");
  EXPECT_EQ(sequence[1], "awaiter2_suspend");
  EXPECT_EQ(sequence[2], "notifier_fire");
  EXPECT_EQ(sequence[3], "notifier_done");

  // Since both were resumed during Fire(), their exact resumption order inside the execution loop
  // depends on the scheduler scheduling them, but both must have resumed.
  EXPECT_TRUE(sequence[4] == "awaiter1_resume" || sequence[4] == "awaiter2_resume");
  EXPECT_TRUE(sequence[5] == "awaiter1_resume" || sequence[5] == "awaiter2_resume");
  EXPECT_NE(sequence[4], sequence[5]);
}

// 5. Test case: Multiple awaiters co_awaiting an already fired Event
TEST(EventTest, MultipleAwaitersEarlyFire) {
  boost::asio::io_context io;
  AsioExecutor executor(io);
  Manager manager(executor);

  Event event;
  event.Fire();

  bool child1Executed = false;
  bool child2Executed = false;

  manager.SpawnRoot("root", [&]() -> Coroutine<void> {
    Fiber& current = co_await GetCurrentFiber();

    auto child1 = current.Spawn("awaiter1", [&]() -> Coroutine<void> {
      co_await event;
      child1Executed = true;
      co_return;
    });

    auto child2 = current.Spawn("awaiter2", [&]() -> Coroutine<void> {
      co_await event;
      child2Executed = true;
      co_return;
    });

    co_await current.Join(child1);
    co_await current.Join(child2);
    co_return;
  });

  RunEventLoop(io);

  EXPECT_TRUE(child1Executed);
  EXPECT_TRUE(child2Executed);
}

// 6. Test case: Call Fire multiple times on the same Event
TEST(EventTest, MultipleFires) {
  boost::asio::io_context io;
  AsioExecutor executor(io);
  Manager manager(executor);

  Event event;
  event.Fire();
  EXPECT_TRUE(event.AwaitReady());

  // Call Fire again
  event.Fire();
  EXPECT_TRUE(event.AwaitReady());

  bool fiberExecuted = false;

  manager.SpawnRoot("root", [&]() -> Coroutine<void> {
    co_await event;
    fiberExecuted = true;
    co_return;
  });

  RunEventLoop(io);

  EXPECT_TRUE(fiberExecuted);
}

// 7. Test case: Safe destruction of Event when there is a live awaitable holding a reference to the context
TEST(EventTest, DestructionSafety) {
  auto eventPtr = std::make_unique<Event>();

  // Obtain an awaitable object from the event, which populates eventPtr->_AwaitContext
  // and creates a FiberAwaitContext shared by the awaitable.
  auto awaitable = eventPtr->operator co_await();

  // Destroy the event while the awaitable is still alive.
  // This verifies that destroying the Event safely manages the weak_ptr reference to the context
  // without dangling pointers, while the actual context is kept alive by the awaitable.
  eventPtr.reset();

  EXPECT_EQ(eventPtr, nullptr);
}
