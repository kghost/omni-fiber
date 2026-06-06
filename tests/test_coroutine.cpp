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

using namespace Omni::Fiber;

namespace {

void RunEventLoop(boost::asio::io_context& io) {
  io.restart();
  io.run();
}

// 1. Helper Coroutines for 3-Level Void Call Suite
Coroutine<void> Level3Void(std::vector<std::string>& sequence) {
  sequence.push_back("Level3Void: Start");
  sequence.push_back("Level3Void: End");
  co_return;
}

Coroutine<void> Level2Void(std::vector<std::string>& sequence) {
  sequence.push_back("Level2Void: Start");
  co_await Level3Void(sequence);
  sequence.push_back("Level2Void: End");
  co_return;
}

Coroutine<void> Level1Void(std::vector<std::string>& sequence) {
  sequence.push_back("Level1Void: Start");
  co_await Level2Void(sequence);
  sequence.push_back("Level1Void: End");
  co_return;
}

// 2. Helper Coroutines for 3-Level Value Propagation Suite
Coroutine<int> Level3Value(int value) { co_return value * 2; }

Coroutine<int> Level2Value(int value) {
  int result = co_await Level3Value(value);
  co_return result + 10;
}

Coroutine<int> Level1Value(int value) {
  int result = co_await Level2Value(value);
  co_return result + 100;
}

// 3. Helper Coroutines for 3-Level Cooperative Suspension
Coroutine<void> Level3Suspend(std::vector<std::string>& sequence, Event<void>& event) {
  sequence.push_back("Level3Suspend: Awaiting event");
  co_await event;
  sequence.push_back("Level3Suspend: Event signaled");
  co_return;
}

Coroutine<void> Level2Suspend(std::vector<std::string>& sequence, Event<void>& event) {
  sequence.push_back("Level2Suspend: Awaiting Level3");
  co_await Level3Suspend(sequence, event);
  sequence.push_back("Level2Suspend: Level3 returned");
  co_return;
}

Coroutine<void> Level1Suspend(std::vector<std::string>& sequence, Event<void>& event) {
  sequence.push_back("Level1Suspend: Awaiting Level2");
  co_await Level2Suspend(sequence, event);
  sequence.push_back("Level1Suspend: Level2 returned");
  co_return;
}

// 4. Recursive Fibonacci Coroutine
Coroutine<int> Fibonacci(int n) {
  if (n <= 1) {
    co_return n;
  }
  int first = co_await Fibonacci(n - 1);
  int second = co_await Fibonacci(n - 2);
  co_return first + second;
}

// 5. Helper Coroutines for 3-Level Exception Propagation
Coroutine<void> Level3Exception() {
  throw std::runtime_error("Exception from Level3");
  co_return;
}

Coroutine<void> Level2Exception() {
  co_await Level3Exception();
  co_return;
}

Coroutine<void> Level1Exception(bool& caught) {
  try {
    co_await Level2Exception();
  } catch (const std::runtime_error& e) {
    if (std::string(e.what()) == "Exception from Level3") {
      caught = true;
    }
  }
  co_return;
}

} // namespace

// Test Case 1: 3-Level Void Call Flow
TEST(CoroutineTest, ThreeLevelsVoid) {
  boost::asio::io_context io;
  AsioExecutor executor(io);
  Manager manager(executor);

  std::vector<std::string> sequence;

  manager.SpawnRoot("root", [&]() -> Coroutine<void> {
    sequence.push_back("root: Start");
    co_await Level1Void(sequence);
    sequence.push_back("root: End");
    co_return;
  });

  RunEventLoop(io);

  ASSERT_EQ(sequence.size(), 8);
  EXPECT_EQ(sequence[0], "root: Start");
  EXPECT_EQ(sequence[1], "Level1Void: Start");
  EXPECT_EQ(sequence[2], "Level2Void: Start");
  EXPECT_EQ(sequence[3], "Level3Void: Start");
  EXPECT_EQ(sequence[4], "Level3Void: End");
  EXPECT_EQ(sequence[5], "Level2Void: End");
  EXPECT_EQ(sequence[6], "Level1Void: End");
  EXPECT_EQ(sequence[7], "root: End");
}

// Test Case 2: 3-Level Value Propagation
TEST(CoroutineTest, ThreeLevelsWithValue) {
  boost::asio::io_context io;
  AsioExecutor executor(io);
  Manager manager(executor);

  int finalResult = 0;

  manager.SpawnRoot("root", [&]() -> Coroutine<void> {
    finalResult = co_await Level1Value(5);
    co_return;
  });

  RunEventLoop(io);

  // Level 3: 5 * 2 = 10
  // Level 2: 10 + 10 = 20
  // Level 1: 20 + 100 = 120
  EXPECT_EQ(finalResult, 120);
}

// Test Case 3: 3-Level Cooperative Suspension & Resumption
TEST(CoroutineTest, ThreeLevelsWithCooperativeSuspend) {
  boost::asio::io_context io;
  AsioExecutor executor(io);
  Manager manager(executor);

  Event<void> event;
  std::vector<std::string> sequence;

  manager.SpawnRoot("root", [&]() -> Coroutine<void> {
    sequence.push_back("root: Spawning Level1");
    Fiber& current = co_await GetCurrentFiber();

    auto child = current.Spawn("child_call_stack", [&]() -> Coroutine<void> {
      co_await Level1Suspend(sequence, event);
      co_return;
    });

    auto signaller = current.Spawn("signaller", [&]() -> Coroutine<void> {
      sequence.push_back("signaller: Signaling event");
      event.Fire();
      co_return;
    });

    co_await current.Join(child);
    co_await current.Join(signaller);
    co_return;
  });

  RunEventLoop(io);

  ASSERT_EQ(sequence.size(), 8);
  EXPECT_EQ(sequence[0], "root: Spawning Level1");
  EXPECT_EQ(sequence[1], "Level1Suspend: Awaiting Level2");
  EXPECT_EQ(sequence[2], "Level2Suspend: Awaiting Level3");
  EXPECT_EQ(sequence[3], "Level3Suspend: Awaiting event");
  EXPECT_EQ(sequence[4], "signaller: Signaling event");
  EXPECT_EQ(sequence[5], "Level3Suspend: Event signaled");
  EXPECT_EQ(sequence[6], "Level2Suspend: Level3 returned");
  EXPECT_EQ(sequence[7], "Level1Suspend: Level2 returned");
}

// Test Case 4: Deep Nested Recursive Execution (Fibonacci)
TEST(CoroutineTest, DeepNestedRecursiveFibonacci) {
  boost::asio::io_context io;
  AsioExecutor executor(io);
  Manager manager(executor);

  int result = 0;

  manager.SpawnRoot("root", [&]() -> Coroutine<void> {
    result = co_await Fibonacci(5);
    co_return;
  });

  RunEventLoop(io);

  // Fib(0) = 0
  // Fib(1) = 1
  // Fib(2) = 1
  // Fib(3) = 2
  // Fib(4) = 3
  // Fib(5) = 5
  EXPECT_EQ(result, 5);
}

// Test Case 5: 3-Level Exception Propagation
TEST(CoroutineTest, NestedExceptionPropagation) {
  boost::asio::io_context io;
  AsioExecutor executor(io);
  Manager manager(executor);

  bool caught = false;

  manager.SpawnRoot("root", [&]() -> Coroutine<void> {
    co_await Level1Exception(caught);
    co_return;
  });

  RunEventLoop(io);

  EXPECT_TRUE(caught);
}
