#include <boost/asio.hpp>
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <vector>

#include "Asio.h"
#include "Coroutine.h"
#include "Event.h"
#include "EventQueue.h"
#include "Fiber.h"
#include "Manager.h"

using namespace Omni::Fiber;

// Helper to run io_context for a short duration or until no work remains
void RunEventLoop(boost::asio::io_context& io) {
  io.restart();
  io.run();
}

// 1. Test Spawning and Basic Fiber Scheduling
TEST(FiberTest, BasicSpawnAndSchedule) {
  boost::asio::io_context io;
  AsioExecutor executor(io);
  Manager manager(executor);

  bool rootExecuted = false;
  bool childExecuted = false;

  manager.SpawnRoot("root", [&]() -> Coroutine<void> {
    rootExecuted = true;
    auto current = Manager::GetCurrentFiber();

    auto child = current->Spawn("child", [&]() -> Coroutine<void> {
      childExecuted = true;
      co_return;
    });

    co_await current->Join(child);
    co_return;
  });

  RunEventLoop(io);

  EXPECT_TRUE(rootExecuted);
  EXPECT_TRUE(childExecuted);
}

// 2. Test Fiber Joining
TEST(FiberTest, FiberJoin) {
  boost::asio::io_context io;
  AsioExecutor executor(io);
  Manager manager(executor);

  std::vector<std::string> order;

  manager.SpawnRoot("root", [&]() -> Coroutine<void> {
    order.push_back("root_start");
    auto current = Manager::GetCurrentFiber();

    auto child = current->Spawn("child", [&]() -> Coroutine<void> {
      order.push_back("child_run");
      co_return;
    });

    order.push_back("root_before_join");
    co_await current->Join(child);
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
  AsioExecutor executor(io);
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
  AsioExecutor executor(io);
  Manager manager(executor);

  Event startEvent;
  bool consumerFinished = false;
  bool producerFinished = false;
  std::vector<std::string> sequence;

  manager.SpawnRoot("root", [&]() -> Coroutine<void> {
    auto current = Manager::GetCurrentFiber();

    // Spawn consumer
    auto consumer = current->Spawn("consumer", [&]() -> Coroutine<void> {
      sequence.push_back("consumer_waiting");
      co_await startEvent;
      sequence.push_back("consumer_resumed");
      consumerFinished = true;
      co_return;
    });

    // Spawn producer
    auto producer = current->Spawn("producer", [&]() -> Coroutine<void> {
      sequence.push_back("producer_running");
      EXPECT_FALSE(consumerFinished);
      startEvent.Set();
      sequence.push_back("producer_signaled");
      producerFinished = true;
      co_return;
    });

    co_await current->Join(consumer);
    co_await current->Join(producer);
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
  AsioExecutor executor(io);
  Manager manager(executor);

  EventQueue<int> queue;
  std::vector<int> received;

  manager.SpawnRoot("root", [&]() -> Coroutine<void> {
    auto current = Manager::GetCurrentFiber();

    // Consumer
    auto consumer = current->Spawn("consumer", [&]() -> Coroutine<void> {
      for (int i = 0; i < 3; ++i) {
        co_await queue;
        received.push_back(queue.PopFront());
      }
      co_return;
    });

    // Producer
    auto producer = current->Spawn("producer", [&]() -> Coroutine<void> {
      queue.Push(10);
      queue.Push(20);
      queue.Push(30);
      co_return;
    });

    co_await current->Join(consumer);
    co_await current->Join(producer);
    co_return;
  });

  RunEventLoop(io);

  ASSERT_EQ(received.size(), 3);
  EXPECT_EQ(received[0], 10);
  EXPECT_EQ(received[1], 20);
  EXPECT_EQ(received[2], 30);
}

// 6. Test Fiber Interruption
TEST(FiberTest, FiberInterruption) {
  boost::asio::io_context io;
  AsioExecutor executor(io);
  Manager manager(executor);

  Event blockEvent;
  bool caughtInterruption = false;
  bool childFinishedGracefully = false;

  manager.SpawnRoot("root", [&]() -> Coroutine<void> {
    auto current = Manager::GetCurrentFiber();

    auto child = current->Spawn("child", [&]() -> Coroutine<void> {
      try {
        co_await blockEvent; // Will suspend forever unless interrupted
        childFinishedGracefully = true;
      } catch (const Fiber::FiberInterrupted&) {
        caughtInterruption = true;
      }
      co_return;
    });

    // Spawn an interrupter
    auto interrupter = current->Spawn("interrupter", [&, child]() -> Coroutine<void> {
      child->Interrupt();
      // Reschedule the child to process the interruption
      child->Schedule();
      co_return;
    });

    co_await current->Join(child);
    co_await current->Join(interrupter);
    co_return;
  });

  RunEventLoop(io);

  EXPECT_TRUE(caughtInterruption);
  EXPECT_FALSE(childFinishedGracefully);
}
