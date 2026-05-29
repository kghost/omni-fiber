#include <chrono>
#include <memory>
#include <string>
#include <utility>

#include <boost/asio.hpp>
#include <gtest/gtest.h>

#include "Asio.hpp"
#include "Coroutine.hpp"
#include "Fiber.hpp"
#include "GetCurrentFiber.hpp"
#include "Manager.hpp"

using namespace Omni::Fiber;

namespace {

void RunEventLoop(boost::asio::io_context& io) {
  io.restart();
  io.run();
}

template <typename CompletionToken>
auto AsyncCustomOp(boost::asio::io_context& io, int x, const std::string& y, CompletionToken&& token) {
  return boost::asio::async_initiate<CompletionToken, void(boost::system::error_code, int, std::string)>(
      [](auto&& handler, boost::asio::io_context& io, int x, std::string y) {
        boost::asio::post(io, [handler = std::move(handler), x, y = std::move(y)]() mutable {
          handler(boost::system::error_code{}, x * 2, y + "_suffix");
        });
      },
      token, std::ref(io), x, y);
}

} // namespace

// Test Case 1: Basic Timer Wait
TEST(AsioTest, BasicTimerWait) {
  boost::asio::io_context io;
  AsioExecutor executor(io);
  Manager manager(executor);

  bool executed = false;

  manager.SpawnRoot("root", [&]() -> Coroutine<void> {
    boost::asio::steady_timer timer(io, std::chrono::milliseconds(50));
    auto startTime = std::chrono::steady_clock::now();

    auto [ec] = co_await timer.async_wait(AsioUseFiber);

    auto endTime = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

    EXPECT_FALSE(ec);
    EXPECT_GE(elapsed, 45); // Should sleep for at least ~50ms
    executed = true;
    co_return;
  });

  RunEventLoop(io);
  EXPECT_TRUE(executed);
}

// Test Case 2: Timer Cancellation
TEST(AsioTest, TimerCancellation) {
  boost::asio::io_context io;
  AsioExecutor executor(io);
  Manager manager(executor);

  bool childExecuted = false;
  bool interrupterExecuted = false;

  manager.SpawnRoot("root", [&]() -> Coroutine<void> {
    Fiber& current = co_await GetCurrentFiber();
    auto timer = std::make_shared<boost::asio::steady_timer>(io, std::chrono::seconds(10));

    auto child = current.Spawn("child", [timer, &childExecuted]() -> Coroutine<void> {
      auto [ec] = co_await timer->async_wait(AsioUseFiber);
      EXPECT_EQ(ec, boost::asio::error::operation_aborted);
      childExecuted = true;
      co_return;
    });

    auto interrupter = current.Spawn("interrupter", [timer, &interrupterExecuted]() -> Coroutine<void> {
      timer->cancel();
      interrupterExecuted = true;
      co_return;
    });

    co_await current.Join(child);
    co_await current.Join(interrupter);
    co_return;
  });

  RunEventLoop(io);
  EXPECT_TRUE(childExecuted);
  EXPECT_TRUE(interrupterExecuted);
}

// Test Case: Cancellation slot Integration
TEST(AsioTest, CancellationSlotIntegration) {
  boost::asio::io_context io;
  AsioExecutor executor(io);
  Manager manager(executor);

  bool childExecuted = false;
  bool interrupterExecuted = false;

  manager.SpawnRoot("root", [&]() -> Coroutine<void> {
    auto sig = std::make_shared<boost::asio::cancellation_signal>();
    auto timer = std::make_shared<boost::asio::steady_timer>(io, std::chrono::seconds(10));

    Fiber& current = co_await GetCurrentFiber();
    auto child = current.Spawn("child", [timer, sig, &childExecuted]() -> Coroutine<void> {
      auto [ec] = co_await timer->async_wait(boost::asio::bind_cancellation_slot(sig->slot(), AsioUseFiber));
      EXPECT_EQ(ec, boost::asio::error::operation_aborted);
      childExecuted = true;
      co_return;
    });

    auto interrupter = current.Spawn("interrupter", [sig, &interrupterExecuted]() -> Coroutine<void> {
      sig->emit(boost::asio::cancellation_type::total);
      interrupterExecuted = true;
      co_return;
    });

    co_await current.Join(child);
    co_await current.Join(interrupter);
    co_return;
  });

  RunEventLoop(io);
  EXPECT_TRUE(childExecuted);
  EXPECT_TRUE(interrupterExecuted);
}

// Test Case 3: Multiple Results Integration
TEST(AsioTest, MultiArgumentCustomAsyncOperation) {
  boost::asio::io_context io;
  AsioExecutor executor(io);
  Manager manager(executor);

  bool executed = false;

  manager.SpawnRoot("root", [&]() -> Coroutine<void> {
    auto [ec, val, str] = co_await AsyncCustomOp(io, 21, "hello", AsioUseFiber);

    EXPECT_FALSE(ec);
    EXPECT_EQ(val, 42);
    EXPECT_EQ(str, "hello_suffix");
    executed = true;
    co_return;
  });

  RunEventLoop(io);
  EXPECT_TRUE(executed);
}

// Test Case 4: TCP Socket Ping-Pong over multiple rounds
TEST(AsioTest, TcpSocketMultipleRounds) {
  boost::asio::io_context io;
  AsioExecutor executor(io);
  Manager manager(executor);

  bool serverDone = false;
  bool clientDone = false;
  const int numRounds = 5;

  using boost::asio::ip::tcp;

  manager.SpawnRoot("root", [&]() -> Coroutine<void> {
    Fiber& current = co_await GetCurrentFiber();

    // Start acceptor
    tcp::acceptor acceptor(io, tcp::endpoint(tcp::v4(), 0));
    tcp::endpoint serverEndpoint = acceptor.local_endpoint();

    // Server Fiber
    auto server = current.Spawn("server", [&]() -> Coroutine<void> {
      tcp::socket socket(io);
      // Accept connection
      auto [ec] = co_await acceptor.async_accept(socket, AsioUseFiber);
      EXPECT_FALSE(ec);

      for (int round = 0; round < numRounds; ++round) {
        // Read "ping" from client
        char readBuf[128] = {0};
        auto [ecRead, bytesRead] = co_await socket.async_read_some(boost::asio::buffer(readBuf), AsioUseFiber);
        EXPECT_FALSE(ecRead);
        std::string msg(readBuf, bytesRead);
        EXPECT_EQ(msg, "ping_" + std::to_string(round));

        // Write "pong" back to client
        std::string response = "pong_" + std::to_string(round);
        auto [ecWrite, bytesWritten] =
            co_await boost::asio::async_write(socket, boost::asio::buffer(response), AsioUseFiber);
        EXPECT_FALSE(ecWrite);
        EXPECT_EQ(bytesWritten, response.size());
      }
      serverDone = true;
      co_return;
    });

    // Client Fiber
    auto client = current.Spawn("client", [&]() -> Coroutine<void> {
      tcp::socket socket(io);
      // Connect to server
      auto [ec] = co_await socket.async_connect(serverEndpoint, AsioUseFiber);
      EXPECT_FALSE(ec);

      for (int round = 0; round < numRounds; ++round) {
        // Write "ping" to server
        std::string msg = "ping_" + std::to_string(round);
        auto [ecWrite, bytesWritten] =
            co_await boost::asio::async_write(socket, boost::asio::buffer(msg), AsioUseFiber);
        EXPECT_FALSE(ecWrite);
        EXPECT_EQ(bytesWritten, msg.size());

        // Read "pong" back from server
        char readBuf[128] = {0};
        auto [ecRead, bytesRead] = co_await socket.async_read_some(boost::asio::buffer(readBuf), AsioUseFiber);
        EXPECT_FALSE(ecRead);
        std::string response(readBuf, bytesRead);
        EXPECT_EQ(response, "pong_" + std::to_string(round));
      }
      clientDone = true;
      co_return;
    });

    co_await current.Join(server);
    co_await current.Join(client);
    co_return;
  });

  RunEventLoop(io);
  EXPECT_TRUE(serverDone);
  EXPECT_TRUE(clientDone);
}
