#include <boost/asio.hpp>
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <vector>

#ifndef NDEBUG
#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sinks/sync_frontend.hpp>
#include <boost/log/sinks/text_ostream_backend.hpp>
#include <sstream>
#endif

#include "Asio.hpp"
#include "Coroutine.hpp"
#include "Event.hpp"
#include "EventQueue.hpp"
#include "Fiber.hpp"
#include "GetCurrentOmniFiber.hpp"
#include "Manager.hpp"
#include "OmniYield.hpp"

using namespace Omni::Fiber;

// Helper to run io_context for a short duration or until no work remains
void RunEventLoop(boost::asio::io_context& io) {
  io.restart();
  io.run();
}

// 1. Test Spawning and Basic Fiber Scheduling
TEST(FiberTest, BasicSpawnAndSchedule) {
  boost::asio::io_context io;
  AsioExecutor executor(io.get_executor());
  Manager manager(executor);

  bool rootExecuted = false;
  bool childExecuted = false;

  manager.SpawnRoot("root", [&]() -> Coroutine<void> {
    rootExecuted = true;
    Fiber& current = co_await GetCurrentOmniFiber();

    auto child = current.Spawn("child", [&]() -> Coroutine<void> {
      childExecuted = true;
      co_return;
    });

    co_await current.Join(child);
    co_return;
  });

  RunEventLoop(io);

  EXPECT_TRUE(rootExecuted);
  EXPECT_TRUE(childExecuted);
}

// 2. Test Fiber Joining
TEST(FiberTest, FiberJoin) {
  boost::asio::io_context io;
  AsioExecutor executor(io.get_executor());
  Manager manager(executor);

  std::vector<std::string> order;

  manager.SpawnRoot("root", [&]() -> Coroutine<void> {
    order.push_back("root_start");
    Fiber& current = co_await GetCurrentOmniFiber();

    auto child = current.Spawn("child", [&]() -> Coroutine<void> {
      order.push_back("child_run");
      co_return;
    });

    order.push_back("root_before_join");
    co_await current.Join(child);
    order.push_back("root_after_join");

    co_return;
  });

  RunEventLoop(io);

  ASSERT_EQ(order.size(), 4);
  EXPECT_EQ(order[0], "root_start");
  EXPECT_EQ(order[1], "root_before_join");
  EXPECT_EQ(order[2], "child_run");
  EXPECT_EQ(order[3], "root_after_join");
}

// 3. Test Coroutine Return Values (non-void and void propagation)
Coroutine<int> AsyncAdd(int a, int b) { co_return a + b; }

TEST(FiberTest, CoroutineReturnValue) {
  boost::asio::io_context io;
  AsioExecutor executor(io.get_executor());
  Manager manager(executor);

  int result = 0;

  manager.SpawnRoot("root", [&]() -> Coroutine<void> {
    result = co_await AsyncAdd(20, 22);
    co_return;
  });

  RunEventLoop(io);

  EXPECT_EQ(result, 42);
}

// 4. Test Event cooperative synchronization
TEST(FiberTest, CooperativeEvent) {
  boost::asio::io_context io;
  AsioExecutor executor(io.get_executor());
  Manager manager(executor);

  Event<void> startEvent;
  bool consumerFinished = false;
  bool producerFinished = false;
  std::vector<std::string> sequence;

  manager.SpawnRoot("root", [&]() -> Coroutine<void> {
    Fiber& current = co_await GetCurrentOmniFiber();

    // Spawn consumer
    auto consumer = current.Spawn("consumer", [&]() -> Coroutine<void> {
      sequence.push_back("consumer_waiting");
      co_await startEvent;
      sequence.push_back("consumer_resumed");
      consumerFinished = true;
      co_return;
    });

    // Spawn producer
    auto producer = current.Spawn("producer", [&]() -> Coroutine<void> {
      sequence.push_back("producer_running");
      EXPECT_FALSE(consumerFinished);
      startEvent.Fire();
      sequence.push_back("producer_signaled");
      producerFinished = true;
      co_return;
    });

    co_await current.Join(consumer);
    co_await current.Join(producer);
    co_return;
  });

  RunEventLoop(io);

  EXPECT_TRUE(consumerFinished);
  EXPECT_TRUE(producerFinished);

  ASSERT_EQ(sequence.size(), 4);
  EXPECT_EQ(sequence[0], "consumer_waiting");
  EXPECT_EQ(sequence[1], "producer_running");
  EXPECT_EQ(sequence[2], "producer_signaled");
  EXPECT_EQ(sequence[3], "consumer_resumed");
}

// 5. Test EventQueue cooperative communication
TEST(FiberTest, CooperativeEventQueue) {
  boost::asio::io_context io;
  AsioExecutor executor(io.get_executor());
  Manager manager(executor);

  EventQueue<int> queue;
  std::vector<int> received;

  manager.SpawnRoot("root", [&]() -> Coroutine<void> {
    Fiber& current = co_await GetCurrentOmniFiber();

    // Consumer
    auto consumer = current.Spawn("consumer", [&]() -> Coroutine<void> {
      for (int i = 0; i < 3; ++i) {
        co_await queue;
        received.push_back(queue.PopFront());
      }
      co_return;
    });

    // Producer
    auto producer = current.Spawn("producer", [&]() -> Coroutine<void> {
      queue.Push(10);
      queue.Push(20);
      queue.Push(30);
      co_return;
    });

    co_await current.Join(consumer);
    co_await current.Join(producer);
    co_return;
  });

  RunEventLoop(io);

  ASSERT_EQ(received.size(), 3);
  EXPECT_EQ(received[0], 10);
  EXPECT_EQ(received[1], 20);
  EXPECT_EQ(received[2], 30);
}

// 7. Test WaitFor basic functionality (first to exit)
TEST(FiberTest, WaitForBasic) {
  boost::asio::io_context io;
  AsioExecutor executor(io.get_executor());
  Manager manager(executor);

  std::vector<std::shared_ptr<Fiber>> finishedOrder;

  manager.SpawnRoot("root", [&]() -> Coroutine<void> {
    Fiber& current = co_await GetCurrentOmniFiber();

    Event<void> child1Event;
    Event<void> child2Event;

    auto child1 = current.Spawn("child1", [&]() -> Coroutine<void> {
      co_await child1Event;
      co_return;
    });

    auto child2 = current.Spawn("child2", [&]() -> Coroutine<void> {
      co_await child2Event;
      co_return;
    });

    // Make child2 finish first
    child2Event.Fire();

    // Since child2 is finished, WaitFor should return child2
    auto firstFinished = co_await current.WaitFor();
    finishedOrder.push_back(firstFinished);

    // Make child1 finish second
    child1Event.Fire();

    std::shared_ptr<Fiber> secondFinished = co_await current.WaitFor();
    finishedOrder.push_back(secondFinished);

    co_return;
  });

  RunEventLoop(io);

  ASSERT_EQ(finishedOrder.size(), 2);
  EXPECT_EQ(finishedOrder[0]->GetName(), "child2");
  EXPECT_EQ(finishedOrder[1]->GetName(), "child1");
}

// 8. Test WaitFor when child has already finished
TEST(FiberTest, WaitForAlreadyFinished) {
  boost::asio::io_context io;
  AsioExecutor executor(io.get_executor());
  Manager manager(executor);

  std::string finishedName;

  manager.SpawnRoot("root", [&]() -> Coroutine<void> {
    Fiber& current = co_await GetCurrentOmniFiber();

    Event<void> resumeEvent1;
    Event<void> resumeEvent2;
    auto child = current.Spawn("early_bird", [&]() -> Coroutine<void> {
      resumeEvent1.Fire();
      resumeEvent2.Fire();
      co_return;
    });

    co_await resumeEvent1;
    co_await resumeEvent2;

    // At this point, child is fully finished.
    // WaitFor should return immediately.
    finishedName = (co_await current.WaitFor())->GetName();
    co_return;
  });

  RunEventLoop(io);

  EXPECT_EQ(finishedName, "early_bird");
}

// 9. Test WaitFor with multiple children already finished
TEST(FiberTest, WaitForMultipleAlreadyFinished) {
  boost::asio::io_context io;
  AsioExecutor executor(io.get_executor());
  Manager manager(executor);

  std::vector<std::string> finishedNames;

  manager.SpawnRoot("root", [&]() -> Coroutine<void> {
    Fiber& current = co_await GetCurrentOmniFiber();

    Event<void> resumeEvent1;
    auto child1 = current.Spawn("first", [&]() -> Coroutine<void> {
      resumeEvent1.Fire();
      co_return;
    });
    co_await resumeEvent1;

    Event<void> resumeEvent2;
    auto child2 = current.Spawn("second", [&]() -> Coroutine<void> {
      resumeEvent2.Fire();
      co_return;
    });
    co_await resumeEvent2;

    // At this point, both children are finished.
    // WaitFor should return both names.
    finishedNames.push_back((co_await current.WaitFor())->GetName());
    finishedNames.push_back((co_await current.WaitFor())->GetName());
    co_return;
  });

  RunEventLoop(io);

  ASSERT_EQ(finishedNames.size(), 2);
  EXPECT_TRUE(std::find(finishedNames.begin(), finishedNames.end(), "first") != finishedNames.end());
  EXPECT_TRUE(std::find(finishedNames.begin(), finishedNames.end(), "second") != finishedNames.end());
}

// 10. Test that Join does not discard finish signals of other children
TEST(FiberTest, JoinInterleavedSignalLossBug) {
  boost::asio::io_context io;
  AsioExecutor executor(io.get_executor());
  Manager manager(executor);

  std::string waitedName;

  manager.SpawnRoot("root", [&]() -> Coroutine<void> {
    Fiber& current = co_await GetCurrentOmniFiber();

    Event<void> event1;
    Event<void> event2;

    auto child1 = current.Spawn("child1", [&]() -> Coroutine<void> {
      co_await event1;
      co_return;
    });

    auto child2 = current.Spawn("child2", [&]() -> Coroutine<void> {
      co_await event2;
      co_return;
    });

    auto child3 = current.Spawn("child3", [&]() -> Coroutine<void> { co_return; });

    // Make child1 finish first
    event1.Fire();

    // Make child2 finish second
    event2.Fire();

    // Now parent calls Join(child2).
    // Join(child2) will pop child1 (finished first) and must not discard it!
    // Then it will pop child2 and return.
    co_await current.Join(child2);

    // Now parent calls WaitFor(). It should immediately find child1 since its signal was stored!
    waitedName = (co_await current.WaitFor())->GetName();

    // Join child3 as well so that no child remains in _Children
    co_await current.WaitFor();
    co_return;
  });

  RunEventLoop(io);

  EXPECT_EQ(waitedName, "child1");
}

// 11. Test Capture and Output of Fiber Callstack via Promise Chain
#ifndef NDEBUG
Coroutine<void> CallstackTrace_C(Event<void>& event) {
  co_await event;
  co_return;
}

Coroutine<void> CallstackTrace_B(Event<void>& event) {
  co_await CallstackTrace_C(event);
  co_return;
}

Coroutine<void> CallstackTrace_A(Event<void>& event) {
  co_await CallstackTrace_B(event);
  co_return;
}

TEST(FiberTest, CallstackTrace) {
  boost::asio::io_context io;
  AsioExecutor executor(io.get_executor());
  Manager manager(executor);

  Event<void> event;
  bool finished = false;

  // Set up Boost.Log capture
  boost::shared_ptr<boost::log::core> logCore = boost::log::core::get();

  // Set filter to debug to ensure our messages pass
  logCore->set_filter(boost::log::trivial::severity >= boost::log::trivial::debug);

  using BackendType = boost::log::sinks::text_ostream_backend;
  boost::shared_ptr<BackendType> backend = boost::make_shared<BackendType>();
  boost::shared_ptr<std::stringstream> logStream(new std::stringstream());
  backend->add_stream(logStream);

  using SinkType = boost::log::sinks::synchronous_sink<BackendType>;
  boost::shared_ptr<SinkType> sink = boost::make_shared<SinkType>(backend);
  logCore->add_sink(sink);

  std::shared_ptr<Fiber> child;

  manager.SpawnRoot("root", [&]() -> Coroutine<void> {
    Fiber& current = co_await GetCurrentOmniFiber();

    child = current.Spawn("trace_child", [&]() -> Coroutine<void> {
      co_await CallstackTrace_A(event);
      finished = true;
      co_return;
    });

    co_await current.Join(child);
    co_return;
  });

  RunEventLoop(io);

  // At this point, both fibers are suspended, so we call DumpAllFibers from the test body!
  manager.DumpAllFibers();

  // Resume and run the event loop to finish
  event.Fire();
  RunEventLoop(io);

  // Clean up boost log capture sink
  logCore->remove_sink(sink);

  EXPECT_TRUE(finished);

  std::string logs = logStream->str();
  // Verify that the callstack logs were output
  EXPECT_NE(logs.find("#0"), std::string::npos);
  EXPECT_NE(logs.find("#1"), std::string::npos);
  EXPECT_NE(logs.find("#2"), std::string::npos);

  // Print logs for manual inspection
  std::cout << "Captured log:\n" << logs << std::endl;
}
#endif

// Test Fiber Yielding and Low-Priority Scheduling Queue
TEST(FiberTest, FiberYieldLowPriority) {
  boost::asio::io_context io;
  AsioExecutor executor(io.get_executor());
  Manager manager(executor);

  std::vector<std::string> order;

  manager.SpawnRoot("root", [&]() -> Coroutine<void> {
    Fiber& current = co_await GetCurrentOmniFiber();

    auto child1 = current.Spawn("child1", [&]() -> Coroutine<void> {
      order.push_back("child1_1");
      co_await OmniYield();
      order.push_back("child1_2");
      co_await OmniYield();
      order.push_back("child1_3");
      co_return;
    });

    auto child2 = current.Spawn("child2", [&]() -> Coroutine<void> {
      order.push_back("child2_1");
      order.push_back("child2_2");
      co_return;
    });

    co_await current.Join(child1);
    co_await current.Join(child2);
    co_return;
  });

  RunEventLoop(io);

  ASSERT_EQ(order.size(), 5);
  EXPECT_EQ(order[0], "child1_1");
  EXPECT_EQ(order[1], "child2_1");
  EXPECT_EQ(order[2], "child2_2");
  EXPECT_EQ(order[3], "child1_2");
  EXPECT_EQ(order[4], "child1_3");
}
