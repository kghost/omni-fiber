#include <boost/asio.hpp>
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <vector>

#include "Asio.hpp"
#include "Coroutine.hpp"
#include "Event.hpp"
#include "Fiber.hpp"
#include "GetCurrentOmniFiber.hpp"
#include "Manager.hpp"
#include "Pipe.hpp"
#include "Select.hpp"
#include "SelectPair.hpp"
#include "SelectPairDynamicList.hpp"

using namespace Omni::Fiber;

namespace {

void RunEventLoop(boost::asio::io_context& io) {
  io.restart();
  io.run();
}

} // namespace

// 1. Heterogeneous: mixed event types and callbacks
TEST(SelectPairDynamicListTest, MixedAwaitables) {
  boost::asio::io_context io;
  AsioExecutor executor(io.get_executor());
  Manager manager(executor);

  Event<void> eventVoid;
  Event<int> eventInt;
  Pipe<std::string> pipe;

  std::vector<std::string> sequence;

  manager.SpawnRoot("root", [&]() -> Coroutine<void> {
    Fiber& current = co_await GetCurrentOmniFiber();

    auto notifier = current.Spawn("notifier", [&]() -> Coroutine<void> {
      sequence.push_back("fire_events");
      eventInt.Fire(42);
      co_await pipe.GetProducer().Put("hello");
      co_return;
    });

    auto consumer = pipe.GetConsumer();
    SelectPairDynamicList list;
    list.Add(eventVoid, [&]() { sequence.push_back("void_callback"); });
    list.Add(eventInt, [&](int val) { sequence.push_back("int_callback_" + std::to_string(val)); });
    list.Add(consumer, [&](auto res) {
      if (res.has_value()) {
        sequence.push_back("pipe_callback_" + res.value());
      }
    });

    sequence.push_back("select_start");
    co_await Select(list);
    sequence.push_back("select_done");

    co_await current.Join(notifier);
    co_await pipe.GetProducer().Shutdown();
    co_return;
  });

  RunEventLoop(io);

  ASSERT_EQ(sequence.size(), 5);
  EXPECT_EQ(sequence[0], "select_start");
  EXPECT_EQ(sequence[1], "fire_events");
  // Both eventInt and pipe consumer callbacks should execute
  EXPECT_TRUE((sequence[2] == "int_callback_42" && sequence[3] == "pipe_callback_hello") ||
              (sequence[2] == "pipe_callback_hello" && sequence[3] == "int_callback_42"));
  EXPECT_EQ(sequence[4], "select_done");
}

// 2. Heterogeneous: Coroutine callbacks
TEST(SelectPairDynamicListTest, CoroutineCallbacks) {
  boost::asio::io_context io;
  AsioExecutor executor(io.get_executor());
  Manager manager(executor);

  Event<void> eventVoid;
  Event<int> eventInt;
  std::vector<std::string> sequence;

  manager.SpawnRoot("root", [&]() -> Coroutine<void> {
    Fiber& current = co_await GetCurrentOmniFiber();

    auto notifier = current.Spawn("notifier", [&]() -> Coroutine<void> {
      sequence.push_back("fire_int");
      eventInt.Fire(100);
      co_return;
    });

    SelectPairDynamicList list;
    list.Add(eventVoid, [&]() -> Coroutine<void> {
      sequence.push_back("void_coro_callback");
      co_return;
    });
    list.Add(eventInt, [&](int val) -> Coroutine<void> {
      sequence.push_back("int_coro_callback_start");
      boost::asio::steady_timer timer(io, std::chrono::milliseconds(20));
      co_await timer.async_wait(AsioUseFiber);
      sequence.push_back("int_coro_callback_end_" + std::to_string(val));
      co_return;
    });

    sequence.push_back("select_start");
    co_await Select(list);
    sequence.push_back("select_done");

    co_await current.Join(notifier);
    co_return;
  });

  RunEventLoop(io);

  ASSERT_EQ(sequence.size(), 5);
  EXPECT_EQ(sequence[0], "select_start");
  EXPECT_EQ(sequence[1], "fire_int");
  EXPECT_EQ(sequence[2], "int_coro_callback_start");
  EXPECT_EQ(sequence[3], "int_coro_callback_end_100");
  EXPECT_EQ(sequence[4], "select_done");
}

// 3. Heterogeneous: RAII cancellation of pending event/pipe registrations
TEST(SelectPairDynamicListTest, RaiiCancellation) {
  boost::asio::io_context io;
  AsioExecutor executor(io.get_executor());
  Manager manager(executor);

  Event<void> eventVoid;
  Pipe<int> pipe;

  manager.SpawnRoot("root", [&]() -> Coroutine<void> {
    Fiber& current = co_await GetCurrentOmniFiber();

    auto notifier = current.Spawn("notifier", [&]() -> Coroutine<void> {
      eventVoid.Fire();
      co_return;
    });

    {
      auto consumer = pipe.GetConsumer();
      SelectPairDynamicList list;
      list.Add(eventVoid, []() {});
      list.Add(consumer, [](auto) {});
      co_await Select(list);
    } // list is destroyed here, and pipe consumer must be cancelled

    co_await current.Join(notifier);
    co_await pipe.GetProducer().Shutdown();
    co_return;
  });

  RunEventLoop(io);

  // Firing / putting data into pipe after list is destroyed should not crash or throw
  EXPECT_NO_THROW(boost::asio::io_context timerIo);
}

// 4. Heterogeneous: SelectPairDynamicList used directly inside Select mixed with another SelectPair
TEST(SelectPairDynamicListTest, UsedDirectlyInsideSelectMixed) {
  boost::asio::io_context io;
  AsioExecutor executor(io.get_executor());
  Manager manager(executor);

  Event<void> event1;
  Event<void> event2;
  Event<void> event3;

  std::vector<std::string> sequence;

  manager.SpawnRoot("root", [&]() -> Coroutine<void> {
    Fiber& current = co_await GetCurrentOmniFiber();

    auto notifier = current.Spawn("notifier", [&]() -> Coroutine<void> {
      sequence.push_back("fire_event2");
      event2.Fire();
      co_return;
    });

    SelectPairDynamicList list;
    list.Add(event1, [&sequence]() { sequence.push_back("callback_event1"); });
    list.Add(event2, [&sequence]() { sequence.push_back("callback_event2"); });

    sequence.push_back("select_start");

    auto [list_result, event3_result] =
        co_await Select(list, SelectPair(event3, [&]() { sequence.push_back("callback_event3"); }));

    sequence.push_back("select_done");

    EXPECT_TRUE(list_result.has_value());
    EXPECT_FALSE(event3_result.has_value());

    co_await current.Join(notifier);
    co_return;
  });

  RunEventLoop(io);

  ASSERT_EQ(sequence.size(), 4);
  EXPECT_EQ(sequence[0], "select_start");
  EXPECT_EQ(sequence[1], "fire_event2");
  EXPECT_EQ(sequence[2], "callback_event2");
  EXPECT_EQ(sequence[3], "select_done");
}
