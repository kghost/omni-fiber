#include <boost/asio.hpp>
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <vector>

#include "Asio.hpp"
#include "Coroutine.hpp"
#include "Event.hpp"
#include "Fiber.hpp"
#include "GetCurrentFiber.hpp"
#include "Manager.hpp"
#include "Select.hpp"
#include "SelectPair.hpp"
#include "SelectPairList.hpp"

using namespace Omni::Fiber;

namespace {

void RunEventLoop(boost::asio::io_context& io) {
  io.restart();
  io.run();
}

} // namespace

// 1. Homogeneous: dynamic size, single event completes
TEST(SelectPairListTest, SingleEventCompletes) {
  boost::asio::io_context io;
  AsioExecutor executor(io.get_executor());
  Manager manager(executor);

  Event<void> event1;
  Event<void> event2;
  Event<void> event3;
  Event<void> event4;
  Event<void> event5;

  std::vector<int> triggered_indices;

  manager.SpawnRoot("root", [&]() -> Coroutine<void> {
    Fiber& current = co_await GetCurrentFiber();

    auto notifier = current.Spawn("notifier", [&]() -> Coroutine<void> {
      event3.Fire(); // Fire index 2
      co_return;
    });

    SelectPairList<Event<void>&, std::function<void()>> list;
    list.Add(event1, [&triggered_indices]() { triggered_indices.push_back(0); });
    list.Add(event2, [&triggered_indices]() { triggered_indices.push_back(1); });
    list.Add(event3, [&triggered_indices]() { triggered_indices.push_back(2); });
    list.Add(event4, [&triggered_indices]() { triggered_indices.push_back(3); });
    list.Add(event5, [&triggered_indices]() { triggered_indices.push_back(4); });

    auto [results] = co_await Select(list);

    EXPECT_EQ(results.size(), 5);
    EXPECT_TRUE(results[2].has_value());
    EXPECT_FALSE(results[0].has_value());
    EXPECT_FALSE(results[1].has_value());
    EXPECT_FALSE(results[3].has_value());
    EXPECT_FALSE(results[4].has_value());

    co_await current.Join(notifier);
    co_return;
  });

  RunEventLoop(io);

  ASSERT_EQ(triggered_indices.size(), 1);
  EXPECT_EQ(triggered_indices[0], 2);
}

// 2. Homogeneous: multiple events complete simultaneously
TEST(SelectPairListTest, MultipleSimultaneousEvents) {
  boost::asio::io_context io;
  AsioExecutor executor(io.get_executor());
  Manager manager(executor);

  std::vector<std::shared_ptr<Event<void>>> events;
  for (int i = 0; i < 4; ++i) {
    events.push_back(std::make_shared<Event<void>>());
  }

  std::vector<int> triggered_indices;

  manager.SpawnRoot("root", [&]() -> Coroutine<void> {
    Fiber& current = co_await GetCurrentFiber();

    auto notifier = current.Spawn("notifier", [&]() -> Coroutine<void> {
      events[1]->Fire();
      events[3]->Fire();
      co_return;
    });

    SelectPairList<Event<void>&, std::function<void()>> list;
    for (size_t i = 0; i < events.size(); ++i) {
      list.Add(*events[i], [&triggered_indices, i]() { triggered_indices.push_back(i); });
    }

    co_await Select(list);

    co_await current.Join(notifier);
    co_return;
  });

  RunEventLoop(io);

  ASSERT_EQ(triggered_indices.size(), 2);
  EXPECT_TRUE(triggered_indices[0] == 1 || triggered_indices[0] == 3);
  EXPECT_TRUE(triggered_indices[1] == 1 || triggered_indices[1] == 3);
}

// 3. Homogeneous: RAII cancellation of pending events
TEST(SelectPairListTest, RaiiCancellation) {
  boost::asio::io_context io;
  AsioExecutor executor(io.get_executor());
  Manager manager(executor);

  std::vector<std::shared_ptr<Event<void>>> events;
  for (int i = 0; i < 3; ++i) {
    events.push_back(std::make_shared<Event<void>>());
  }

  manager.SpawnRoot("root", [&]() -> Coroutine<void> {
    Fiber& current = co_await GetCurrentFiber();

    auto notifier = current.Spawn("notifier", [&]() -> Coroutine<void> {
      events[0]->Fire();
      co_return;
    });

    {
      SelectPairList<Event<void>&, std::function<void()>> list;
      for (size_t i = 0; i < events.size(); ++i) {
        list.Add(*events[i], []() {});
      }
      co_await Select(list);
    } // list is destroyed here, and awaiters for event1, event2 should be cancelled

    co_await current.Join(notifier);
    co_return;
  });

  RunEventLoop(io);

  // Firing events after the list is destroyed should not crash or throw
  EXPECT_NO_THROW(events[1]->Fire());
  EXPECT_NO_THROW(events[2]->Fire());
}

// 4. Homogeneous: callbacks returning non-void values
TEST(SelectPairListTest, CallbackReturnValues) {
  boost::asio::io_context io;
  AsioExecutor executor(io.get_executor());
  Manager manager(executor);

  std::vector<std::shared_ptr<Event<int>>> events;
  for (int i = 0; i < 3; ++i) {
    events.push_back(std::make_shared<Event<int>>());
  }

  bool executed = false;

  manager.SpawnRoot("root", [&]() -> Coroutine<void> {
    Fiber& current = co_await GetCurrentFiber();

    auto notifier = current.Spawn("notifier", [&]() -> Coroutine<void> {
      events[1]->Fire(100);
      co_return;
    });

    SelectPairList<Event<int>&, std::function<int(int)>> list;
    for (size_t i = 0; i < events.size(); ++i) {
      list.Add(*events[i], [](int val) -> int { return val * 2; });
    }

    auto [results] = co_await Select(list);

    EXPECT_EQ(results.size(), 3);
    EXPECT_FALSE(results[0].has_value());
    EXPECT_EQ(results[1].value(), 200);
    EXPECT_FALSE(results[2].has_value());

    executed = true;
    co_await current.Join(notifier);
    co_return;
  });

  RunEventLoop(io);
  EXPECT_TRUE(executed);
}

// 5. Homogeneous: SelectPairList used inside Select
TEST(SelectPairListTest, UsedInsideSelect) {
  boost::asio::io_context io;
  AsioExecutor executor(io.get_executor());
  Manager manager(executor);

  Event<void> event1;
  Event<void> event2;
  Event<void> event3;

  std::vector<std::string> sequence;

  manager.SpawnRoot("root", [&]() -> Coroutine<void> {
    Fiber& current = co_await GetCurrentFiber();

    auto notifier = current.Spawn("notifier", [&]() -> Coroutine<void> {
      sequence.push_back("fire_event2");
      event2.Fire();
      co_return;
    });

    SelectPairList<Event<void>&, std::function<void()>> list;
    list.Add(event1, [&sequence]() { sequence.push_back("callback_event1"); });
    list.Add(event2, [&sequence]() { sequence.push_back("callback_event2"); });

    sequence.push_back("select_start");

    co_await Select(SelectPair(list,
                               [&](auto results) {
                                 sequence.push_back("list_callback");
                                 EXPECT_EQ(results.size(), 2);
                                 EXPECT_FALSE(results[0].has_value());
                                 EXPECT_TRUE(results[1].has_value());
                               }),
                    SelectPair(event3, [&]() { sequence.push_back("callback_event3"); }));

    sequence.push_back("select_done");

    co_await current.Join(notifier);
    co_return;
  });

  RunEventLoop(io);

  ASSERT_EQ(sequence.size(), 4);
  EXPECT_EQ(sequence[0], "select_start");
  EXPECT_EQ(sequence[1], "fire_event2");
  EXPECT_EQ(sequence[2], "list_callback");
  EXPECT_EQ(sequence[3], "select_done");
}

// 6. Homogeneous: SelectPairList used directly inside Select mixed with another SelectPair
TEST(SelectPairListTest, UsedDirectlyInsideSelectMixed) {
  boost::asio::io_context io;
  AsioExecutor executor(io.get_executor());
  Manager manager(executor);

  Event<void> event1;
  Event<void> event2;
  Event<void> event3;

  std::vector<std::string> sequence;

  manager.SpawnRoot("root", [&]() -> Coroutine<void> {
    Fiber& current = co_await GetCurrentFiber();

    auto notifier = current.Spawn("notifier", [&]() -> Coroutine<void> {
      sequence.push_back("fire_event2");
      event2.Fire();
      co_return;
    });

    SelectPairList<Event<void>&, std::function<void()>> list;
    list.Add(event1, [&sequence]() { sequence.push_back("callback_event1"); });
    list.Add(event2, [&sequence]() { sequence.push_back("callback_event2"); });

    sequence.push_back("select_start");

    auto [list_results, event3_result] =
        co_await Select(list, SelectPair(event3, [&]() { sequence.push_back("callback_event3"); }));

    sequence.push_back("select_done");

    EXPECT_EQ(list_results.size(), 2);
    EXPECT_FALSE(list_results[0].has_value());
    EXPECT_TRUE(list_results[1].has_value());
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
