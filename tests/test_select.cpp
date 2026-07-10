#include <memory>
#include <string>
#include <vector>

#include <boost/asio.hpp>
#include <gtest/gtest.h>

#include "Asio.hpp"
#include "Coroutine.hpp"
#include "Event.hpp"
#include "Fiber.hpp"
#include "GetCurrentOmniFiber.hpp"
#include "Manager.hpp"
#include "OmniYield.hpp"
#include "Pipe.hpp"
#include "Select.hpp"
#include "SelectPair.hpp"

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
  AsioExecutor executor(io.get_executor());
  Manager manager(executor);

  Event<void> event1;
  Event<void> event2;
  std::vector<std::string> sequence;

  manager.SpawnRoot("root", [&]() -> Coroutine<void> {
    Fiber& current = co_await GetCurrentOmniFiber();

    auto notifier = current.Spawn("notifier", [&]() -> Coroutine<void> {
      sequence.push_back("fire1");
      event1.Fire();
      co_return;
    });

    sequence.push_back("select_start");
    co_await Select(SelectPair(event1, [&]() -> void { sequence.push_back("callback1"); }),
                    SelectPair(event2, [&]() -> void { sequence.push_back("callback2"); }));
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
  AsioExecutor executor(io.get_executor());
  Manager manager(executor);

  Event<void> event1;
  Event<void> event2;
  std::vector<std::string> sequence;

  manager.SpawnRoot("root", [&]() -> Coroutine<void> {
    Fiber& current = co_await GetCurrentOmniFiber();

    auto notifier = current.Spawn("notifier", [&]() -> Coroutine<void> {
      sequence.push_back("fire_both");
      event1.Fire();
      event2.Fire();
      co_return;
    });

    sequence.push_back("select_start");
    co_await Select(SelectPair(event1, [&]() -> void { sequence.push_back("callback1"); }),
                    SelectPair(event2, [&]() -> void { sequence.push_back("callback2"); }));
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
  AsioExecutor executor(io.get_executor());
  Manager manager(executor);

  Event<void> event1;
  Event<void> event2;
  std::vector<std::string> sequence;

  event2.Fire(); // Fire event2 early

  manager.SpawnRoot("root", [&]() -> Coroutine<void> {
    sequence.push_back("select_start");
    co_await Select(SelectPair(event1, [&]() -> void { sequence.push_back("callback1"); }),
                    SelectPair(event2, [&]() -> void { sequence.push_back("callback2"); }));
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
  AsioExecutor executor(io.get_executor());
  Manager manager(executor);

  Event<int> event1;
  Event<std::string> event2;
  std::vector<std::string> sequence;

  manager.SpawnRoot("root", [&]() -> Coroutine<void> {
    Fiber& current = co_await GetCurrentOmniFiber();

    auto notifier = current.Spawn("notifier", [&]() -> Coroutine<void> {
      event1.Fire(42);
      co_return;
    });

    co_await Select(
        SelectPair(event1, [&](int result) -> void { sequence.push_back("callback1_val_" + std::to_string(result)); }),
        SelectPair(event2, [&](const std::string& result) -> void { sequence.push_back("callback2_val_" + result); }));

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
  AsioExecutor executor(io.get_executor());
  Manager manager(executor);

  Event<void> event1;
  Event<void> event2;
  std::vector<std::string> sequence;

  manager.SpawnRoot("root", [&]() -> Coroutine<void> {
    Fiber& current = co_await GetCurrentOmniFiber();

    auto notifier1 = current.Spawn("notifier1", [&]() -> Coroutine<void> {
      sequence.push_back("fire1");
      event1.Fire();
      co_return;
    });

    co_await Select(SelectPair(event1, [&]() -> void { sequence.push_back("callback1"); }),
                    SelectPair(event2, [&]() -> void { sequence.push_back("callback2"); }));

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
  AsioExecutor executor(io.get_executor());
  Manager manager(executor);

  Pipe<int> pipe;

  std::vector<std::string> sequence;

  manager.SpawnRoot("root", [&]() -> Coroutine<void> {
    Fiber& current = co_await GetCurrentOmniFiber();

    auto notifier = current.Spawn("notifier", [&]() -> Coroutine<void> {
      co_await pipe.GetProducer().Put(100);
      co_return;
    });

    co_await Select(SelectPair(pipe.GetConsumer(), [&](auto result) -> auto {
      if (result.has_value()) {
        sequence.push_back("pipe_val_" + std::to_string(result.value()));
      }
    }));

    co_await current.Join(notifier);
    co_await pipe.GetProducer().Shutdown();
    co_return;
  });

  RunEventLoop(io);

  ASSERT_EQ(sequence.size(), 1);
  EXPECT_EQ(sequence[0], "pipe_val_100");
}

// 7. Test case: Selecting on a temporary (rvalue) awaitable
TEST(SelectTest, PipeConsumerSelectTemporary) {
  boost::asio::io_context io;
  AsioExecutor executor(io.get_executor());
  Manager manager(executor);

  Pipe<int> pipe;

  std::vector<std::string> sequence;

  manager.SpawnRoot("root", [&]() -> Coroutine<void> {
    Fiber& current = co_await GetCurrentOmniFiber();

    auto notifier = current.Spawn("notifier", [&]() -> Coroutine<void> {
      co_await pipe.GetProducer().Put(200);
      co_return;
    });

    co_await Select(SelectPair(pipe.GetConsumer(), [&](auto result) -> auto {
      if (result.has_value()) {
        sequence.push_back("pipe_temp_val_" + std::to_string(result.value()));
      }
    }));

    co_await current.Join(notifier);
    co_await pipe.GetProducer().Shutdown();
    co_return;
  });

  RunEventLoop(io);

  ASSERT_EQ(sequence.size(), 1);
  EXPECT_EQ(sequence[0], "pipe_temp_val_" + std::to_string(200));
}

// 8. Test case: Coroutine callbacks with single event
TEST(SelectTest, CoroutineCallbacks) {
  boost::asio::io_context io;
  AsioExecutor executor(io.get_executor());
  Manager manager(executor);

  Event<void> event1;
  Event<int> event2;
  Event<void> done_event1;
  std::vector<std::string> sequence;

  manager.SpawnRoot("root", [&]() -> Coroutine<void> {
    Fiber& current = co_await GetCurrentOmniFiber();

    auto notifier = current.Spawn("notifier", [&]() -> Coroutine<void> {
      sequence.push_back("fire1");
      event1.Fire();
      co_await done_event1;
      sequence.push_back("fire2");
      event2.Fire(42);
      co_return;
    });

    sequence.push_back("select_start");
    co_await Select(SelectPair(event1,
                               [&]() -> Coroutine<void> {
                                 sequence.push_back("coro_callback1_start");
                                 done_event1.Fire();
                                 boost::asio::steady_timer timer(io, std::chrono::milliseconds(20));
                                 co_await timer.async_wait(AsioUseFiber);
                                 sequence.push_back("coro_callback1_end");
                                 co_return;
                               }),
                    SelectPair(event2, [&](int result) -> Coroutine<void> {
                      sequence.push_back("coro_callback2_val_" + std::to_string(result));
                      co_return;
                    }));
    sequence.push_back("select_done");

    co_await current.Join(notifier);
    co_return;
  });

  RunEventLoop(io);

  ASSERT_EQ(sequence.size(), 6);
  EXPECT_EQ(sequence[0], "select_start");
  EXPECT_EQ(sequence[1], "fire1");
  EXPECT_EQ(sequence[2], "coro_callback1_start");
  EXPECT_EQ(sequence[3], "fire2");
  EXPECT_EQ(sequence[4], "coro_callback1_end");
  EXPECT_EQ(sequence[5], "select_done");
}

// 9. Test case: Coroutine callbacks executed sequentially
TEST(SelectTest, CoroutineCallbacksSimultaneous) {
  boost::asio::io_context io;
  AsioExecutor executor(io.get_executor());
  Manager manager(executor);

  Event<void> event1;
  Event<int> event2;
  std::vector<std::string> sequence;

  manager.SpawnRoot("root", [&]() -> Coroutine<void> {
    Fiber& current = co_await GetCurrentOmniFiber();

    auto notifier = current.Spawn("notifier", [&]() -> Coroutine<void> {
      sequence.push_back("fire_both");
      event1.Fire();
      event2.Fire(100);
      co_return;
    });

    sequence.push_back("select_start");
    co_await Select(SelectPair(event1,
                               [&]() -> Coroutine<void> {
                                 sequence.push_back("coro_callback1_start");
                                 boost::asio::steady_timer timer(io, std::chrono::milliseconds(20));
                                 co_await timer.async_wait(AsioUseFiber);
                                 sequence.push_back("coro_callback1_end");
                                 co_return;
                               }),
                    SelectPair(event2, [&](int result) -> Coroutine<void> {
                      sequence.push_back("coro_callback2_val_" + std::to_string(result));
                      co_return;
                    }));
    sequence.push_back("select_done");

    co_await current.Join(notifier);
    co_return;
  });

  RunEventLoop(io);

  ASSERT_EQ(sequence.size(), 6);
  EXPECT_EQ(sequence[0], "select_start");
  EXPECT_EQ(sequence[1], "fire_both");
  EXPECT_EQ(sequence[2], "coro_callback1_start");
  EXPECT_EQ(sequence[3], "coro_callback1_end");
  EXPECT_EQ(sequence[4], "coro_callback2_val_100");
  EXPECT_EQ(sequence[5], "select_done");
}

// 10. Test case: Select fiber event and asio timer event (timer completing first)
TEST(SelectTest, FiberEventAndAsioTimerTimerCompletesFirst) {
  boost::asio::io_context io;
  AsioExecutor executor(io.get_executor());
  Manager manager(executor);

  Event<void> event1;
  boost::asio::steady_timer timer(io, std::chrono::milliseconds(20));
  std::vector<std::string> sequence;

  manager.SpawnRoot("root", [&]() -> Coroutine<void> {
    sequence.push_back("select_start");
    co_await Select(SelectPair(event1, [&]() -> void { sequence.push_back("callback_event"); }),
                    SelectPair(timer.async_wait(AsioUseFiber), AsioApply([&](auto ec) -> auto {
                                 EXPECT_FALSE(ec);
                                 sequence.push_back("callback_timer triggered");
                               })));
    sequence.push_back("select_done");
    co_return;
  });

  RunEventLoop(io);

  ASSERT_EQ(sequence.size(), 3);
  EXPECT_EQ(sequence[0], "select_start");
  EXPECT_EQ(sequence[1], "callback_timer triggered");
  EXPECT_EQ(sequence[2], "select_done");
}

// 11. Test case: Select fiber event and asio timer event (fiber event completing first)
TEST(SelectTest, FiberEventAndAsioTimerFiberEventCompletesFirst) {
  boost::asio::io_context io;
  AsioExecutor executor(io.get_executor());
  Manager manager(executor);

  Event<void> event1;
  boost::asio::steady_timer timer(io, std::chrono::seconds(5));
  std::vector<std::string> sequence;

  manager.SpawnRoot("root", [&]() -> Coroutine<void> {
    Fiber& current = co_await GetCurrentOmniFiber();

    auto notifier = current.Spawn("notifier", [&]() -> Coroutine<void> {
      boost::asio::steady_timer waitTimer(io, std::chrono::milliseconds(20));
      co_await waitTimer.async_wait(AsioUseFiber);
      sequence.push_back("fire_event");
      event1.Fire();
      co_return;
    });

    sequence.push_back("select_start");
    co_await Select(SelectPair(event1, [&]() -> void { sequence.push_back("callback_event"); }),
                    SelectPair(timer.async_wait(AsioUseFiber),
                               AsioApply([&](auto ec) -> auto { sequence.push_back("callback_timer_" + ec.message()); })));
    sequence.push_back("select_done");

    timer.cancel();

    co_await current.Join(notifier);
    co_return;
  });

  RunEventLoop(io);

  ASSERT_EQ(sequence.size(), 4);
  EXPECT_EQ(sequence[0], "select_start");
  EXPECT_EQ(sequence[1], "fire_event");
  EXPECT_EQ(sequence[2], "callback_event");
  EXPECT_EQ(sequence[3], "select_done");
}

// 12. Test case: Select fiber event and asio timer event (fiber event already early fired)
TEST(SelectTest, FiberEventAndAsioTimerFiberEventEarlyFired) {
  boost::asio::io_context io;
  AsioExecutor executor(io.get_executor());
  Manager manager(executor);

  Event<void> event1;
  boost::asio::steady_timer timer(io, std::chrono::seconds(5));
  std::vector<std::string> sequence;

  event1.Fire(); // fire early

  manager.SpawnRoot("root", [&]() -> Coroutine<void> {
    sequence.push_back("select_start");
    co_await Select(SelectPair(event1, [&]() -> void { sequence.push_back("callback_event"); }),
                    SelectPair(timer.async_wait(AsioUseFiber),
                               AsioApply([&](auto ec) -> auto { sequence.push_back("callback_timer_" + ec.message()); })));
    sequence.push_back("select_done");
    timer.cancel();
    co_return;
  });

  RunEventLoop(io);

  ASSERT_EQ(sequence.size(), 3);
  EXPECT_EQ(sequence[0], "select_start");
  EXPECT_EQ(sequence[1], "callback_event");
  EXPECT_EQ(sequence[2], "select_done");
}

// 13. Test case: Select pipe consumer and asio timer (pipe completing first)
TEST(SelectTest, PipeAndAsioTimerPipeCompletesFirst) {
  boost::asio::io_context io;
  AsioExecutor executor(io.get_executor());
  Manager manager(executor);

  Pipe<int> pipe;
  boost::asio::steady_timer timer(io, std::chrono::seconds(5));
  std::vector<std::string> sequence;

  manager.SpawnRoot("root", [&]() -> Coroutine<void> {
    Fiber& current = co_await GetCurrentOmniFiber();

    auto notifier = current.Spawn("notifier", [&]() -> Coroutine<void> {
      boost::asio::steady_timer waitTimer(io, std::chrono::milliseconds(20));
      co_await waitTimer.async_wait(AsioUseFiber);
      co_await pipe.GetProducer().Put(300);
      co_return;
    });

    sequence.push_back("select_start");
    co_await Select(SelectPair(pipe.GetConsumer(),
                               [&](auto result) -> auto {
                                 if (result.has_value()) {
                                   sequence.push_back("callback_pipe_" + std::to_string(result.value()));
                                 }
                               }),
                    SelectPair(timer.async_wait(AsioUseFiber),
                               AsioApply([&](auto ec) -> auto { sequence.push_back("callback_timer_" + ec.message()); })));
    sequence.push_back("select_done");

    timer.cancel();

    co_await current.Join(notifier);
    co_await pipe.GetProducer().Shutdown();
    co_return;
  });

  RunEventLoop(io);

  ASSERT_EQ(sequence.size(), 3);
  EXPECT_EQ(sequence[0], "select_start");
  EXPECT_EQ(sequence[1], "callback_pipe_300");
  EXPECT_EQ(sequence[2], "select_done");
}

// 14. Test case: Select pipe consumer and asio timer (timer completing first)
TEST(SelectTest, PipeAndAsioTimerTimerCompletesFirst) {
  boost::asio::io_context io;
  AsioExecutor executor(io.get_executor());
  Manager manager(executor);

  Pipe<int> pipe;
  auto consumer = pipe.GetConsumer();
  boost::asio::steady_timer timer(io, std::chrono::milliseconds(20));
  std::vector<std::string> sequence;

  manager.SpawnRoot("root", [&]() -> Coroutine<void> {
    sequence.push_back("select_start");
    co_await Select(SelectPair(consumer,
                               [&](auto result) -> auto {
                                 if (result.has_value()) {
                                   sequence.push_back("callback_pipe_" + std::to_string(result.value()));
                                 }
                               }),
                    SelectPair(timer.async_wait(AsioUseFiber), AsioApply([&](auto ec) -> auto {
                                 EXPECT_FALSE(ec);
                                 sequence.push_back("callback_timer triggered");
                               })));
    sequence.push_back("select_done");
    co_await pipe.GetProducer().Shutdown();
    co_return;
  });

  RunEventLoop(io);

  ASSERT_EQ(sequence.size(), 3);
  EXPECT_EQ(sequence[0], "select_start");
  EXPECT_EQ(sequence[1], "callback_timer triggered");
  EXPECT_EQ(sequence[2], "select_done");
}

// 15. Test case: Select returns a tuple of callback results
TEST(SelectTest, SelectReturnTupleResults) {
  boost::asio::io_context io;
  AsioExecutor executor(io.get_executor());
  Manager manager(executor);

  Event<void> event1;
  Event<void> event2;
  Event<int> event3;
  Event<int> event4;
  Event<void> event5;
  Event<void> event6;

  bool executed = false;

  manager.SpawnRoot("root", [&]() -> Coroutine<void> {
    Fiber& current = co_await GetCurrentOmniFiber();

    auto notifier = current.Spawn("notifier", [&]() -> Coroutine<void> {
      event1.Fire();
      event3.Fire(100);
      event5.Fire();
      event6.Fire();
      co_return;
    });

    auto results = co_await Select(SelectPair(event1, []() -> void {}), SelectPair(event2, []() -> void {}),
                                   SelectPair(event3, [](int x) -> int { return x / 10; }),
                                   SelectPair(event4, [](int x) -> std::string { return "should not run"; }),
                                   SelectPair(event5, []() -> Coroutine<double> { co_return 5.5; }),
                                   SelectPair(event6, []() -> Coroutine<void> { co_return; }));

    // Verify types of results
    using EvNotReady = AwaiterTraits<decltype(std::declval<Event<void>&>().operator co_await())>::AwaiterNotReady;
    using EIntNotReady = AwaiterTraits<decltype(std::declval<Event<int>&>().operator co_await())>::AwaiterNotReady;
    static_assert(std::is_same_v<decltype(results),
                                 std::tuple<std::expected<void, EvNotReady>, std::expected<void, EvNotReady>,
                                            std::expected<int, EIntNotReady>, std::expected<std::string, EIntNotReady>,
                                            std::expected<double, EvNotReady>, std::expected<void, EvNotReady>>>);

    // Verify values of results
    EXPECT_TRUE(std::get<0>(results).has_value());
    EXPECT_FALSE(std::get<1>(results).has_value());
    EXPECT_EQ(std::get<2>(results).value(), 10);
    EXPECT_FALSE(std::get<3>(results).has_value());
    EXPECT_EQ(std::get<4>(results).value(), 5.5);
    EXPECT_TRUE(std::get<5>(results).has_value());

    executed = true;
    co_await current.Join(notifier);
    co_return;
  });

  RunEventLoop(io);
  EXPECT_TRUE(executed);
}

// 16. Test case: Select trigger same event in callback
TEST(SelectTest, SelectTriggerEventInCallback) {
  boost::asio::io_context io;
  AsioExecutor executor(io.get_executor());
  Manager manager(executor);

  Event<void> event1;
  Event<void> event2;

  bool executed = false;

  manager.SpawnRoot("root", [&]() -> Coroutine<void> {
    Fiber& current = co_await GetCurrentOmniFiber();

    auto selector = current.Spawn("selector", [&]() -> Coroutine<void> {
      auto results =
          co_await Select(SelectPair(event1, [&]() -> void { event2.Fire(); }), SelectPair(event2, []() -> void {}));
      // Verify types of results
      using EvNotReady = AwaiterTraits<decltype(std::declval<Event<void>&>().operator co_await())>::AwaiterNotReady;
      static_assert(std::is_same_v<decltype(results),
                                   std::tuple<std::expected<void, EvNotReady>, std::expected<void, EvNotReady>>>);

      // Verify values of results
      EXPECT_TRUE(std::get<0>(results).has_value());
      EXPECT_FALSE(std::get<1>(results).has_value());
    });

    co_await OmniYield();

    auto notifier = current.Spawn("notifier", [&]() -> Coroutine<void> {
      event1.Fire();
      co_return;
    });

    executed = true;
    co_await current.Join(selector);
    co_await current.Join(notifier);
    co_return;
  });

  RunEventLoop(io);
  EXPECT_TRUE(executed);
}
