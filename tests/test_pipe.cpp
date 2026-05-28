#include <memory>
#include <utility>
#include <vector>

#include <boost/asio.hpp>
#include <gtest/gtest.h>

#include "Asio.hpp"
#include "Coroutine.hpp"
#include "Fiber.hpp"
#include "GetCurrentFiber.hpp"
#include "Manager.hpp"
#include "Pipe.hpp"

using namespace Omni::Fiber;

namespace {

void RunEventLoop(boost::asio::io_context& io) {
  io.restart();
  io.run();
}

} // namespace

// 1. Test case: Initial state of a newly created Pipe
TEST(PipeTest, InitialState) {
  Pipe<int> pipe;
  auto producer = pipe.GetProducer();
  auto consumer = pipe.GetConsumer();

  EXPECT_TRUE(producer.AwaitReady());
  EXPECT_FALSE(consumer.AwaitReady());
}

// 2. Test case: Basic transmission of data without suspending (direct write & read)
TEST(PipeTest, BasicPutAndGet) {
  boost::asio::io_context io;
  AsioExecutor executor(io);
  Manager manager(executor);

  Pipe<int> pipe;
  auto producer = pipe.GetProducer();
  auto consumer = pipe.GetConsumer();

  bool executed = false;

  manager.SpawnRoot("root", [&]() -> Coroutine<void> {
    Fiber& current = co_await GetCurrentFiber();

    auto producerFiber = current.Spawn("producer", [&]() -> Coroutine<void> {
      EXPECT_TRUE(producer.AwaitReady());
      co_await producer.Put(42);
      EXPECT_TRUE(producer.AwaitReady());
      executed = true;
      co_return;
    });

    auto consumerFiber = current.Spawn("consumer", [&]() -> Coroutine<void> {
      EXPECT_TRUE(consumer.AwaitReady());
      auto data = co_await consumer;
      EXPECT_TRUE(data.has_value());
      if (!data.has_value()) {
        co_return;
      }
      EXPECT_EQ(*data, 42);
      co_return;
    });

    co_await current.Join(producerFiber);
    co_await current.Join(consumerFiber);
    co_return;
  });

  RunEventLoop(io);
  EXPECT_TRUE(executed);
}

// 3. Test case: Basic close of pipe
TEST(PipeTest, BasicClose) {
  boost::asio::io_context io;
  AsioExecutor executor(io);
  Manager manager(executor);

  Pipe<int> pipe;
  auto producer = pipe.GetProducer();
  auto consumer = pipe.GetConsumer();

  bool executed = false;

  manager.SpawnRoot("root", [&]() -> Coroutine<void> {
    Fiber& current = co_await GetCurrentFiber();

    auto producerFiber = current.Spawn("producer", [&]() -> Coroutine<void> {
      producer.Close();
      executed = true;
      co_return;
    });

    auto consumerFiber = current.Spawn("consumer", [&]() -> Coroutine<void> {
      EXPECT_TRUE(consumer.AwaitReady());
      auto data = co_await consumer;
      EXPECT_FALSE(data.has_value());
      co_return;
    });

    co_await current.Join(producerFiber);
    co_await current.Join(consumerFiber);
    co_return;
  });

  RunEventLoop(io);
  EXPECT_TRUE(executed);
}

// 4. Test case: Producer suspensions due to full buffer
TEST(PipeTest, ProducerSuspension) {
  boost::asio::io_context io;
  AsioExecutor executor(io);
  Manager manager(executor);

  Pipe<int> pipe;
  auto producer = pipe.GetProducer();
  auto consumer = pipe.GetConsumer();

  std::vector<std::string> sequence;

  manager.SpawnRoot("root", [&]() -> Coroutine<void> {
    Fiber& current = co_await GetCurrentFiber();

    auto producerFiber = current.Spawn("producer", [&]() -> Coroutine<void> {
      sequence.push_back("prod_put_1");
      co_await producer.Put(1);

      sequence.push_back("prod_put_2");
      co_await producer.Put(2);

      sequence.push_back("prod_done");
      co_return;
    });

    auto consumerFiber = current.Spawn("consumer", [&]() -> Coroutine<void> {
      sequence.push_back("cons_read_1");
      auto val1 = co_await consumer;
      EXPECT_TRUE(val1.has_value());
      if (!val1.has_value()) {
        co_return;
      }
      EXPECT_EQ(*val1, 1);

      sequence.push_back("cons_read_2");
      auto val2 = co_await consumer;
      EXPECT_TRUE(val2.has_value());
      if (!val2.has_value()) {
        co_return;
      }
      EXPECT_EQ(*val2, 2);

      sequence.push_back("cons_done");
      co_return;
    });

    co_await current.Join(producerFiber);
    co_await current.Join(consumerFiber);
    co_return;
  });

  RunEventLoop(io);

  ASSERT_EQ(sequence.size(), 6);
  EXPECT_EQ(sequence[0], "prod_put_1");
  EXPECT_EQ(sequence[1], "cons_read_1");
  EXPECT_EQ(sequence[2], "cons_read_2");
  EXPECT_EQ(sequence[3], "prod_put_2");
  EXPECT_EQ(sequence[4], "cons_done");
  EXPECT_EQ(sequence[5], "prod_done");
}

// 5. Test case: Consumer suspensions due to empty buffer
TEST(PipeTest, ConsumerSuspension) {
  boost::asio::io_context io;
  AsioExecutor executor(io);
  Manager manager(executor);

  Pipe<int> pipe;
  auto producer = pipe.GetProducer();
  auto consumer = pipe.GetConsumer();

  std::vector<std::string> sequence;

  manager.SpawnRoot("root", [&]() -> Coroutine<void> {
    Fiber& current = co_await GetCurrentFiber();

    auto consumerFiber = current.Spawn("consumer", [&]() -> Coroutine<void> {
      sequence.push_back("cons_read_1");
      auto val1 = co_await consumer;
      sequence.push_back("cons_got_1");
      EXPECT_TRUE(val1.has_value());
      if (!val1.has_value()) {
        co_return;
      }
      EXPECT_EQ(*val1, 100);

      sequence.push_back("cons_read_2");
      auto val2 = co_await consumer;
      sequence.push_back("cons_got_2");
      EXPECT_FALSE(val2.has_value());

      sequence.push_back("cons_done");
      co_return;
    });

    auto producerFiber = current.Spawn("producer", [&]() -> Coroutine<void> {
      sequence.push_back("prod_put_1");
      co_await producer.Put(100);

      sequence.push_back("prod_close");
      producer.Close();

      sequence.push_back("prod_done");
      co_return;
    });

    co_await current.Join(consumerFiber);
    co_await current.Join(producerFiber);
    co_return;
  });

  RunEventLoop(io);

  ASSERT_EQ(sequence.size(), 8);
  EXPECT_EQ(sequence[0], "cons_read_1");
  EXPECT_EQ(sequence[1], "prod_put_1");
  EXPECT_EQ(sequence[2], "cons_got_1");
  EXPECT_EQ(sequence[3], "cons_read_2");
  EXPECT_EQ(sequence[4], "prod_close");
  EXPECT_EQ(sequence[5], "prod_done");
  EXPECT_EQ(sequence[6], "cons_got_2");
  EXPECT_EQ(sequence[7], "cons_done");
}

// 6. Test case: Destruction safety
TEST(PipeTest, DestructionSafety) {
  auto pipePtr = std::make_unique<Pipe<int>>();
  auto producer = pipePtr->GetProducer();

  // Obtain an awaitable object from the producer, which creates a context
  auto awaitable = producer.Put(42);

  // Destroy the pipe while the awaitable is still alive
  pipePtr.reset();

  EXPECT_EQ(pipePtr, nullptr);
}

// 7. Test case: Pipe supports move-only objects
TEST(PipeTest, MoveOnlyObject) {
  boost::asio::io_context io;
  AsioExecutor executor(io);
  Manager manager(executor);

  Pipe<std::unique_ptr<int>> pipe;
  auto producer = pipe.GetProducer();
  auto consumer = pipe.GetConsumer();

  bool executed = false;

  manager.SpawnRoot("root", [&]() -> Coroutine<void> {
    Fiber& current = co_await GetCurrentFiber();

    auto producerFiber = current.Spawn("producer", [&]() -> Coroutine<void> {
      auto data = std::make_unique<int>(1337);
      co_await producer.Put(std::move(data));
      executed = true;
      co_return;
    });

    auto consumerFiber = current.Spawn("consumer", [&]() -> Coroutine<void> {
      auto val = co_await consumer;
      EXPECT_TRUE(val.has_value());
      if (val.has_value()) {
        EXPECT_NE(*val, nullptr);
        if (*val != nullptr) {
          EXPECT_EQ(**val, 1337);
        }
      }
      co_return;
    });

    co_await current.Join(producerFiber);
    co_await current.Join(consumerFiber);
    co_return;
  });

  RunEventLoop(io);
  EXPECT_TRUE(executed);
}
