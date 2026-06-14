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
  boost::asio::io_context io;
  AsioExecutor executor(io.get_executor());
  Manager manager(executor);

  Pipe<int> pipe;
  manager.SpawnRoot("root", [&]() -> Coroutine<void> {
    EXPECT_TRUE(pipe.GetProducer().AwaitReady());
    EXPECT_FALSE(pipe.GetConsumer().AwaitReady());
    co_await pipe.GetProducer().Close();
    co_return;
  });
  RunEventLoop(io);
}

// 2. Test case: Basic transmission of data without suspending (direct write & read)
TEST(PipeTest, BasicPutAndGet) {
  boost::asio::io_context io;
  AsioExecutor executor(io.get_executor());
  Manager manager(executor);

  Pipe<int> pipe;

  bool executed = false;

  manager.SpawnRoot("root", [&]() -> Coroutine<void> {
    Fiber& current = co_await GetCurrentFiber();

    auto producerFiber = current.Spawn("producer", [&]() -> Coroutine<void> {
      EXPECT_TRUE(pipe.GetProducer().AwaitReady());
      co_await pipe.GetProducer().Put(42);
      executed = true;
      co_return;
    });

    auto consumerFiber = current.Spawn("consumer", [&]() -> Coroutine<void> {
      EXPECT_TRUE(pipe.GetConsumer().AwaitReady());
      auto data = co_await pipe.GetConsumer();
      EXPECT_TRUE(data.has_value());
      if (!data.has_value()) {
        co_return;
      }
      EXPECT_EQ(*data, 42);
      co_return;
    });

    co_await current.Join(producerFiber);
    co_await current.Join(consumerFiber);
    co_await pipe.GetProducer().Close();
    co_return;
  });

  RunEventLoop(io);
  EXPECT_TRUE(executed);
}

// 3. Test case: Basic close of pipe
TEST(PipeTest, BasicClose) {
  boost::asio::io_context io;
  AsioExecutor executor(io.get_executor());
  Manager manager(executor);

  Pipe<int> pipe;

  bool executed = false;

  manager.SpawnRoot("root", [&]() -> Coroutine<void> {
    Fiber& current = co_await GetCurrentFiber();

    auto producerFiber = current.Spawn("producer", [&]() -> Coroutine<void> {
      co_await pipe.GetProducer().Close();
      executed = true;
      co_return;
    });

    auto consumerFiber = current.Spawn("consumer", [&]() -> Coroutine<void> {
      EXPECT_TRUE(pipe.GetConsumer().AwaitReady());
      auto data = co_await pipe.GetConsumer();
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
  AsioExecutor executor(io.get_executor());
  Manager manager(executor);

  Pipe<int> pipe;

  std::vector<std::string> sequence;

  manager.SpawnRoot("root", [&]() -> Coroutine<void> {
    Fiber& current = co_await GetCurrentFiber();

    auto producerFiber = current.Spawn("producer", [&]() -> Coroutine<void> {
      sequence.push_back("prod_put_1");
      co_await pipe.GetProducer().Put(1);

      sequence.push_back("prod_put_2");
      co_await pipe.GetProducer().Put(2);

      sequence.push_back("prod_done");
      co_return;
    });

    auto consumerFiber = current.Spawn("consumer", [&]() -> Coroutine<void> {
      sequence.push_back("cons_read_1");
      auto val1 = co_await pipe.GetConsumer();
      EXPECT_TRUE(val1.has_value());
      if (!val1.has_value()) {
        co_return;
      }
      EXPECT_EQ(*val1, 1);

      sequence.push_back("cons_read_2");
      auto val2 = co_await pipe.GetConsumer();
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
    co_await pipe.GetProducer().Close();
    co_return;
  });

  RunEventLoop(io);

  ASSERT_EQ(sequence.size(), 6);
  EXPECT_EQ(sequence[0], "prod_put_1");
  EXPECT_EQ(sequence[1], "prod_put_2");
  EXPECT_EQ(sequence[2], "cons_read_1");
  EXPECT_EQ(sequence[3], "cons_read_2");
  EXPECT_EQ(sequence[4], "prod_done");
  EXPECT_EQ(sequence[5], "cons_done");
}

// 5. Test case: Consumer suspensions due to empty buffer
TEST(PipeTest, ConsumerSuspension) {
  boost::asio::io_context io;
  AsioExecutor executor(io.get_executor());
  Manager manager(executor);

  Pipe<int> pipe;

  std::vector<std::string> sequence;

  manager.SpawnRoot("root", [&]() -> Coroutine<void> {
    Fiber& current = co_await GetCurrentFiber();

    auto consumerFiber = current.Spawn("consumer", [&]() -> Coroutine<void> {
      sequence.push_back("cons_read_1");
      auto val1 = co_await pipe.GetConsumer();
      sequence.push_back("cons_got_1");
      EXPECT_TRUE(val1.has_value());
      if (!val1.has_value()) {
        co_return;
      }
      EXPECT_EQ(*val1, 100);

      sequence.push_back("cons_read_2");
      auto val2 = co_await pipe.GetConsumer();
      sequence.push_back("cons_got_2");
      EXPECT_FALSE(val2.has_value());

      sequence.push_back("cons_done");
      co_return;
    });

    auto producerFiber = current.Spawn("producer", [&]() -> Coroutine<void> {
      sequence.push_back("prod_put_1");
      co_await pipe.GetProducer().Put(100);

      sequence.push_back("prod_close");
      co_await pipe.GetProducer().Close();

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
  EXPECT_EQ(sequence[2], "prod_close");
  EXPECT_EQ(sequence[3], "cons_got_1");
  EXPECT_EQ(sequence[4], "cons_read_2");
  EXPECT_EQ(sequence[5], "prod_done");
  EXPECT_EQ(sequence[6], "cons_got_2");
  EXPECT_EQ(sequence[7], "cons_done");
}

// 7. Test case: Pipe supports move-only objects
TEST(PipeTest, MoveOnlyObject) {
  boost::asio::io_context io;
  AsioExecutor executor(io.get_executor());
  Manager manager(executor);

  Pipe<std::unique_ptr<int>> pipe;

  bool executed = false;

  manager.SpawnRoot("root", [&]() -> Coroutine<void> {
    Fiber& current = co_await GetCurrentFiber();

    auto producerFiber = current.Spawn("producer", [&]() -> Coroutine<void> {
      auto data = std::make_unique<int>(1337);
      co_await pipe.GetProducer().Put(std::move(data));
      executed = true;
      co_return;
    });

    auto consumerFiber = current.Spawn("consumer", [&]() -> Coroutine<void> {
      auto val = co_await pipe.GetConsumer();
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
    co_await pipe.GetProducer().Close();
    co_return;
  });

  RunEventLoop(io);
  EXPECT_TRUE(executed);
}
