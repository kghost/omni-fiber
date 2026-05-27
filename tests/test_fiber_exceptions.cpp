#include <boost/asio.hpp>
#include <gtest/gtest.h>
#include <memory>
#include <string>

#include "Asio.hpp"
#include "Coroutine.hpp"
#include "EventQueue.hpp"
#include "Fiber.hpp"
#include "FiberException.hpp"
#include "GetCurrentFiber.hpp"
#include "Manager.hpp"

using namespace Omni::Fiber;

namespace {

// Helper to run io_context for a short duration or until no work remains
void RunEventLoop(boost::asio::io_context& io) {
  io.restart();
  io.run();
}

Coroutine<void> Level3Exception() {
  throw std::runtime_error("Exception from deep coroutine stack");
  co_return;
}

Coroutine<void> Level2Exception() {
  co_await Level3Exception();
  co_return;
}

Coroutine<void> Level1Exception() {
  co_await Level2Exception();
  co_return;
}

} // namespace

// 1. Test that child fiber exception propagates to parent fiber.Join
TEST(FiberExceptionTest, ChildExceptionPropagatesToJoin) {
  boost::asio::io_context io;
  AsioExecutor executor(io);
  Manager manager(executor);

  bool exceptionCaught = false;

  manager.SpawnRoot("root", [&]() -> Coroutine<void> {
    Fiber& current = co_await GetCurrentFiber();

    auto child = current.Spawn("child", [&]() -> Coroutine<void> {
      throw std::runtime_error("Test exception in child");
      co_return;
    });

    try {
      co_await current.Join(child);
    } catch (const FiberException& e) {
      exceptionCaught = true;
      EXPECT_EQ(e._Fiber, child);
      try {
        std::rethrow_exception(e._InnerException);
      } catch (const std::runtime_error& inner) {
        EXPECT_STREQ(inner.what(), "Test exception in child");
      } catch (...) {
        EXPECT_TRUE(false) << "Unexpected inner exception type";
      }
    } catch (...) {
      EXPECT_TRUE(false) << "Expected FiberException";
    }

    co_return;
  });

  RunEventLoop(io);
  EXPECT_TRUE(exceptionCaught);
}

// 2. Test that child fiber exception propagates to parent fiber.WaitFor
TEST(FiberExceptionTest, ChildExceptionPropagatesToWaitFor) {
  boost::asio::io_context io;
  AsioExecutor executor(io);
  Manager manager(executor);

  bool exceptionCaught = false;

  manager.SpawnRoot("root", [&]() -> Coroutine<void> {
    Fiber& current = co_await GetCurrentFiber();

    auto child = current.Spawn("child", [&]() -> Coroutine<void> {
      throw std::runtime_error("Test exception in child for WaitFor");
      co_return;
    });

    try {
      co_await current.WaitFor();
    } catch (const FiberException& e) {
      exceptionCaught = true;
      EXPECT_EQ(e._Fiber, child);
      try {
        std::rethrow_exception(e._InnerException);
      } catch (const std::runtime_error& inner) {
        EXPECT_STREQ(inner.what(), "Test exception in child for WaitFor");
      } catch (...) {
        EXPECT_TRUE(false) << "Unexpected inner exception type";
      }
    } catch (...) {
      EXPECT_TRUE(false) << "Expected FiberException";
    }

    co_return;
  });

  RunEventLoop(io);
  EXPECT_TRUE(exceptionCaught);
}

// 3. Test that root fiber exception propagates to Manager->Run()
TEST(FiberExceptionTest, RootExceptionPropagatesToManagerRun) {
  boost::asio::io_context io;
  AsioExecutor executor(io);
  Manager manager(executor);

  manager.SpawnRoot("root", [&]() -> Coroutine<void> {
    throw std::runtime_error("Test exception in root");
    co_return;
  });

  bool exceptionCaught = false;
  try {
    RunEventLoop(io);
  } catch (const FiberException& e) {
    exceptionCaught = true;
    EXPECT_EQ(e._Fiber->GetName(), "root");
    try {
      std::rethrow_exception(e._InnerException);
    } catch (const std::runtime_error& inner) {
      EXPECT_STREQ(inner.what(), "Test exception in root");
    } catch (...) {
      EXPECT_TRUE(false) << "Unexpected inner exception type";
    }
  } catch (...) {
    EXPECT_TRUE(false) << "Expected FiberException";
  }

  EXPECT_TRUE(exceptionCaught);
}

// 4. Test exception propagation through deep coroutine call stack of root fiber directly to Manager::Run
TEST(FiberExceptionTest, DeepCoroutineStackException) {
  boost::asio::io_context io;
  AsioExecutor executor(io);
  Manager manager(executor);

  manager.SpawnRoot("root", [&]() -> Coroutine<void> {
    co_await Level1Exception();
    co_return;
  });

  bool exceptionCaught = false;
  try {
    RunEventLoop(io);
  } catch (const FiberException& e) {
    exceptionCaught = true;
    EXPECT_EQ(e._Fiber->GetName(), "root");
    try {
      std::rethrow_exception(e._InnerException);
    } catch (const std::runtime_error& inner) {
      EXPECT_STREQ(inner.what(), "Exception from deep coroutine stack");
    } catch (...) {
      EXPECT_TRUE(false) << "Unexpected inner exception type";
    }
  } catch (...) {
    EXPECT_TRUE(false) << "Expected FiberException";
  }

  EXPECT_TRUE(exceptionCaught);
}

// 5. Test multi-level nested exception propagation across grandchild, child, and root fibers
TEST(FiberExceptionTest, NestedExceptionPropagation) {
  boost::asio::io_context io;
  AsioExecutor executor(io);
  Manager manager(executor);

  manager.SpawnRoot("root", [&]() -> Coroutine<void> {
    Fiber& current = co_await GetCurrentFiber();

    auto child = current.Spawn("child", [&]() -> Coroutine<void> {
      Fiber& childFiber = co_await GetCurrentFiber();

      auto grandchild = childFiber.Spawn("grandchild", [&]() -> Coroutine<void> {
        throw std::runtime_error("Grandchild error");
        co_return;
      });

      // Join grandchild. This throws FiberException (wrapping grandchild's runtime_error).
      // Since we don't catch it here, it will propagate out of the child fiber.
      co_await childFiber.Join(grandchild);
      co_return;
    });

    // Join child. This throws FiberException (wrapping child's unhandled exception).
    // Since we don't catch it here, it will propagate out of the root fiber.
    co_await current.Join(child);
    co_return;
  });

  bool exceptionCaught = false;
  try {
    RunEventLoop(io);
  } catch (const FiberException& e) {
    exceptionCaught = true;
    EXPECT_EQ(e._Fiber->GetName(), "root");
    try {
      std::rethrow_exception(e._InnerException);
    } catch (const FiberException& childException) {
      EXPECT_EQ(childException._Fiber->GetName(), "child");
      try {
        std::rethrow_exception(childException._InnerException);
      } catch (const FiberException& grandchildException) {
        EXPECT_EQ(grandchildException._Fiber->GetName(), "grandchild");
        try {
          std::rethrow_exception(grandchildException._InnerException);
        } catch (const std::runtime_error& inner) {
          EXPECT_STREQ(inner.what(), "Grandchild error");
        } catch (...) {
          EXPECT_TRUE(false) << "Grandchild inner exception was not runtime_error";
        }
      } catch (...) {
        EXPECT_TRUE(false) << "Grandchild exception was not FiberException";
      }
    } catch (...) {
      EXPECT_TRUE(false) << "Child exception was not FiberException";
    }
  } catch (...) {
    EXPECT_TRUE(false) << "Expected FiberException";
  }

  EXPECT_TRUE(exceptionCaught);
}

// 6. Test that caught fiber exceptions do not propagate to the manager
TEST(FiberExceptionTest, CaughtExceptionDoesNotPropagate) {
  boost::asio::io_context io;
  AsioExecutor executor(io);
  Manager manager(executor);

  bool exceptionCaught = false;
  bool parentCompleted = false;

  manager.SpawnRoot("root", [&]() -> Coroutine<void> {
    Fiber& current = co_await GetCurrentFiber();

    auto child = current.Spawn("child", [&]() -> Coroutine<void> {
      throw std::runtime_error("Child error caught by parent");
      co_return;
    });

    try {
      co_await current.Join(child);
    } catch (const FiberException& e) {
      exceptionCaught = true;
      EXPECT_EQ(e._Fiber, child);
    }

    parentCompleted = true;
    co_return;
  });

  EXPECT_NO_THROW(RunEventLoop(io));

  EXPECT_TRUE(exceptionCaught);
  EXPECT_TRUE(parentCompleted);
}
