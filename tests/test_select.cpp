#include <memory>
#include <string>
#include <vector>

#include <boost/asio.hpp>
#include <gtest/gtest.h>

#include "Asio.hpp"
#include "Coroutine.hpp"
#include "Event.hpp"
#include "Fiber.hpp"
#include "GetCurrentFiber.hpp"
#include "Manager.hpp"
#include "Pipe.hpp"
#include "Select.hpp"

using namespace Omni::Fiber;

namespace {

void RunEventLoop(boost::asio::io_context& io) {
  io.restart();
  io.run();
}

} // namespace

// 1. Test case: Single event completing amongst multiple
TEST(SelectTest, SingleEventCompletes) {
  boost::asio::io_context io;
  AsioExecutor executor(io);
  Manager manager(executor);

  Event<> event1;
  Event<> event2;
  std::vector<std::string> sequence;

  manager.SpawnRoot("root", [&]() -> Coroutine<void> {
    Fiber& current = co_await GetCurrentFiber();

    auto notifier = current.Spawn("notifier", [&]() -> Coroutine<void> {
      sequence.push_back("fire1");
      event1.Fire();
      co_return;
    });

    sequence.push_back("select_start");
    co_await Select(SelectPair(event1, [&]() { sequence.push_back("callback1"); }),
                    SelectPair(event2, [&]() { sequence.push_back("callback2"); }));
    sequence.push_back("select_done");

    co_await current.Join(notifier);
    co_return;
  });

  RunEventLoop(io);

  ASSERT_EQ(sequence.size(), 4);
  EXPECT_EQ(sequence[0], "select_start");
  EXPECT_EQ(sequence[1], "fire1");
  EXPECT_EQ(sequence[2], "callback1");
  EXPECT_EQ(sequence[3], "select_done");
}

// 2. Test case: Multiple simultaneous events completed
TEST(SelectTest, MultipleSimultaneousEvents) {
  boost::asio::io_context io;
  AsioExecutor executor(io);
  Manager manager(executor);

  Event<> event1;
  Event<> event2;
  std::vector<std::string> sequence;

  manager.SpawnRoot("root", [&]() -> Coroutine<void> {
    Fiber& current = co_await GetCurrentFiber();

    auto notifier = current.Spawn("notifier", [&]() -> Coroutine<void> {
      sequence.push_back("fire_both");
      event1.Fire();
      event2.Fire();
      co_return;
    });

    sequence.push_back("select_start");
    co_await Select(SelectPair(event1, [&]() { sequence.push_back("callback1"); }),
                    SelectPair(event2, [&]() { sequence.push_back("callback2"); }));
    sequence.push_back("select_done");

    co_await current.Join(notifier);
    co_return;
  });

  RunEventLoop(io);

  ASSERT_EQ(sequence.size(), 5);
  EXPECT_EQ(sequence[0], "select_start");
  EXPECT_EQ(sequence[1], "fire_both");
  // Both callbacks should execute because both events are ready when the fiber resumes
  EXPECT_TRUE((sequence[2] == "callback1" && sequence[3] == "callback2") ||
              (sequence[2] == "callback2" && sequence[3] == "callback1"));
  EXPECT_EQ(sequence[4], "select_done");
}

// 3. Test case: Early-fired events (events fired before Select is co_awaited)
TEST(SelectTest, EarlyFiredEvents) {
  boost::asio::io_context io;
  AsioExecutor executor(io);
  Manager manager(executor);

  Event<> event1;
  Event<> event2;
  std::vector<std::string> sequence;

  event2.Fire(); // Fire event2 early

  manager.SpawnRoot("root", [&]() -> Coroutine<void> {
    sequence.push_back("select_start");
    co_await Select(SelectPair(event1, [&]() { sequence.push_back("callback1"); }),
                    SelectPair(event2, [&]() { sequence.push_back("callback2"); }));
    sequence.push_back("select_done");
    co_return;
  });

  RunEventLoop(io);

  ASSERT_EQ(sequence.size(), 3);
  EXPECT_EQ(sequence[0], "select_start");
  EXPECT_EQ(sequence[1], "callback2");
  EXPECT_EQ(sequence[2], "select_done");
}

// 4. Test case: Heterogeneous event types and callbacks with/without arguments
TEST(SelectTest, HeterogeneousCallbackArguments) {
  boost::asio::io_context io;
  AsioExecutor executor(io);
  Manager manager(executor);

  Event<int> event1;
  Event<std::string> event2;
  std::vector<std::string> sequence;

  manager.SpawnRoot("root", [&]() -> Coroutine<void> {
    Fiber& current = co_await GetCurrentFiber();

    auto notifier = current.Spawn("notifier", [&]() -> Coroutine<void> {
      event1.Fire(42);
      co_return;
    });

    co_await Select(
        SelectPair(event1, [&](int result) { sequence.push_back("callback1_val_" + std::to_string(result)); }),
        SelectPair(event2, [&](const std::string& result) { sequence.push_back("callback2_val_" + result); }));

    co_await current.Join(notifier);
    co_return;
  });

  RunEventLoop(io);

  ASSERT_EQ(sequence.size(), 1);
  EXPECT_EQ(sequence[0], "callback1_val_42");
}

// 5. Test case: Clean RAII cancellation of unfinished awaitables
TEST(SelectTest, CleanRaiiCancellation) {
  boost::asio::io_context io;
  AsioExecutor executor(io);
  Manager manager(executor);

  Event<> event1;
  Event<> event2;
  std::vector<std::string> sequence;

  manager.SpawnRoot("root", [&]() -> Coroutine<void> {
    Fiber& current = co_await GetCurrentFiber();

    auto notifier1 = current.Spawn("notifier1", [&]() -> Coroutine<void> {
      sequence.push_back("fire1");
      event1.Fire();
      co_return;
    });

    co_await Select(SelectPair(event1, [&]() { sequence.push_back("callback1"); }),
                    SelectPair(event2, [&]() { sequence.push_back("callback2"); }));

    co_await current.Join(notifier1);
    co_return;
  });

  RunEventLoop(io);

  // Now, event2 was never completed, but the Select has returned.
  // The temporary SelectAwaiter is destroyed, which must have de-registered event2.
  // We can fire event2 now, and ensure it is safely handled without any active waiter or crash.
  EXPECT_NO_THROW(event2.Fire());
}

// 6. Test case: Pipe consumer select integration
TEST(SelectTest, PipeConsumerSelect) {
  boost::asio::io_context io;
  AsioExecutor executor(io);
  Manager manager(executor);

  Pipe<int> pipe;
  auto producer = pipe.GetProducer();
  auto consumer = pipe.GetConsumer();

  std::vector<std::string> sequence;

  manager.SpawnRoot("root", [&]() -> Coroutine<void> {
    Fiber& current = co_await GetCurrentFiber();

    auto notifier = current.Spawn("notifier", [&]() -> Coroutine<void> {
      co_await producer.Put(100);
      co_return;
    });

    co_await Select(SelectPair(consumer, [&](auto result) {
      if (result.has_value()) {
        sequence.push_back("pipe_val_" + std::to_string(result.value()));
      }
    }));

    co_await current.Join(notifier);
    co_return;
  });

  RunEventLoop(io);

  ASSERT_EQ(sequence.size(), 1);
  EXPECT_EQ(sequence[0], "pipe_val_100");
}

// 7. Test case: Selecting on a temporary (rvalue) awaitable
TEST(SelectTest, PipeConsumerSelectTemporary) {
  boost::asio::io_context io;
  AsioExecutor executor(io);
  Manager manager(executor);

  Pipe<int> pipe;
  auto producer = pipe.GetProducer();

  std::vector<std::string> sequence;

  manager.SpawnRoot("root", [&]() -> Coroutine<void> {
    Fiber& current = co_await GetCurrentFiber();

    auto notifier = current.Spawn("notifier", [&]() -> Coroutine<void> {
      co_await producer.Put(200);
      co_return;
    });

    co_await Select(SelectPair(pipe.GetConsumer(), [&](auto result) {
      if (result.has_value()) {
        sequence.push_back("pipe_temp_val_" + std::to_string(result.value()));
      }
    }));

    co_await current.Join(notifier);
    co_return;
  });

  RunEventLoop(io);

  ASSERT_EQ(sequence.size(), 1);
  EXPECT_EQ(sequence[0], "pipe_temp_val_" + std::to_string(200));
}

