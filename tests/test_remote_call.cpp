#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <boost/asio.hpp>
#include <gtest/gtest.h>

#include "Asio.hpp"
#include "Coroutine.hpp"
#include "Event.hpp"
#include "Fiber.hpp"
#include "GetCurrentFiber.hpp"
#include "Manager.hpp"
#include "RemoteCall.hpp"
#include "Select.hpp"

using namespace Omni::Fiber;

namespace {

void RunEventLoop(boost::asio::io_context& io) {
  io.restart();
  io.run();
}

} // namespace

// 1. Test case: Basic Value RPC (returns a value)
TEST(RemoteCallTest, BasicValueCall) {
  boost::asio::io_context io;
  AsioExecutor executor(io);
  Manager manager(executor);

  RemoteCall rc;
  bool executed = false;

  manager.SpawnRoot("root", [&]() -> Coroutine<void> {
    Fiber& current = co_await GetCurrentFiber();

    auto server = current.Spawn("server", [&]() -> Coroutine<void> {
      co_await rc.Serve();
      co_return;
    });

    auto client = current.Spawn("client", [&]() -> Coroutine<void> {
      // Send rpc that takes int and returns std::string
      auto reply = co_await rc.Call([]() -> Coroutine<std::string> { co_return "hello " + std::to_string(42); });
      EXPECT_TRUE(reply.has_value());
      EXPECT_EQ(reply.value(), "hello 42");
      executed = true;
      co_await rc.Close();
      co_return;
    });

    co_await current.Join(server);
    co_await current.Join(client);
    co_return;
  });

  RunEventLoop(io);
  EXPECT_TRUE(executed);
}

// 2. Test case: Void Reply RPC (returns void)
TEST(RemoteCallTest, VoidReplyCall) {
  boost::asio::io_context io;
  AsioExecutor executor(io);
  Manager manager(executor);

  RemoteCall rc;
  bool server_side_ran = false;
  bool executed = false;

  manager.SpawnRoot("root", [&]() -> Coroutine<void> {
    Fiber& current = co_await GetCurrentFiber();

    auto server = current.Spawn("server", [&]() -> Coroutine<void> {
      co_await rc.Serve();
      co_return;
    });

    auto client = current.Spawn("client", [&]() -> Coroutine<void> {
      co_await rc.Call([&]() -> Coroutine<void> {
        server_side_ran = true;
        co_return;
      });
      executed = true;
      co_await rc.Close();
      co_return;
    });

    co_await current.Join(server);
    co_await current.Join(client);
    co_return;
  });

  RunEventLoop(io);
  EXPECT_TRUE(server_side_ran);
  EXPECT_TRUE(executed);
}

// 3. Test case: Move-Only Request/Reply
TEST(RemoteCallTest, MoveOnlyTypes) {
  boost::asio::io_context io;
  AsioExecutor executor(io);
  Manager manager(executor);

  RemoteCall rc;
  bool executed = false;

  manager.SpawnRoot("root", [&]() -> Coroutine<void> {
    Fiber& current = co_await GetCurrentFiber();

    auto server = current.Spawn("server", [&]() -> Coroutine<void> {
      co_await rc.Serve();
      co_return;
    });

    auto client = current.Spawn("client", [&]() -> Coroutine<void> {
      auto req = std::make_unique<int>(1337);
      // RPC returns move-only type (unique_ptr)
      auto reply = co_await rc.Call([r = std::move(req)]() -> Coroutine<std::unique_ptr<int>> {
        EXPECT_EQ(*r, 1337);
        co_return std::make_unique<int>(*r + 1);
      });
      EXPECT_TRUE(reply.has_value());
      EXPECT_NE(reply.value(), nullptr);
      EXPECT_EQ(*reply.value(), 1338);
      executed = true;
      co_await rc.Close();
      co_return;
    });

    co_await current.Join(server);
    co_await current.Join(client);
    co_return;
  });

  RunEventLoop(io);
  EXPECT_TRUE(executed);
}

// 4. Test case: Multiple Sequential Calls
TEST(RemoteCallTest, SequentialCalls) {
  boost::asio::io_context io;
  AsioExecutor executor(io);
  Manager manager(executor);

  RemoteCall rc;
  std::vector<int> replies;

  manager.SpawnRoot("root", [&]() -> Coroutine<void> {
    Fiber& current = co_await GetCurrentFiber();

    auto server = current.Spawn("server", [&]() -> Coroutine<void> {
      co_await rc.Serve();
      co_return;
    });

    auto client = current.Spawn("client", [&]() -> Coroutine<void> {
      for (int i = 0; i < 5; ++i) {
        auto reply = co_await rc.Call([i]() -> Coroutine<int> { co_return i * 2; });
        EXPECT_TRUE(reply.has_value());
        replies.push_back(reply.value());
      }
      co_await rc.Close();
      co_return;
    });

    co_await current.Join(server);
    co_await current.Join(client);
    co_return;
  });

  RunEventLoop(io);
  ASSERT_EQ(replies.size(), 5);
  for (int i = 0; i < 5; ++i) {
    EXPECT_EQ(replies[i], i * 2);
  }
}

// 5. Test case: Multiple Concurrent Calls
TEST(RemoteCallTest, ConcurrentCalls) {
  boost::asio::io_context io;
  AsioExecutor executor(io);
  Manager manager(executor);

  RemoteCall rc;
  std::vector<int> replies;

  manager.SpawnRoot("root", [&]() -> Coroutine<void> {
    Fiber& current = co_await GetCurrentFiber();

    auto server = current.Spawn("server", [&]() -> Coroutine<void> {
      co_await rc.Serve();
      co_return;
    });

    auto client1 = current.Spawn("client1", [&]() -> Coroutine<void> {
      auto reply = co_await rc.Call([]() -> Coroutine<int> { co_return 100; });
      EXPECT_TRUE(reply.has_value());
      replies.push_back(reply.value());
      co_return;
    });

    auto client2 = current.Spawn("client2", [&]() -> Coroutine<void> {
      auto reply = co_await rc.Call([]() -> Coroutine<int> { co_return 200; });
      EXPECT_TRUE(reply.has_value());
      replies.push_back(reply.value());
      co_return;
    });

    co_await current.Join(client1);
    co_await current.Join(client2);
    co_await rc.Close();
    co_await current.Join(server);
    co_return;
  });

  RunEventLoop(io);
  ASSERT_EQ(replies.size(), 2);
  EXPECT_TRUE((replies[0] == 100 && replies[1] == 200) || (replies[0] == 200 && replies[1] == 100));
}

// 6. Test case: Server Shutdown
TEST(RemoteCallTest, ServerShutdown) {
  boost::asio::io_context io;
  AsioExecutor executor(io);
  Manager manager(executor);

  RemoteCall rc;
  bool server_exited = false;

  manager.SpawnRoot("root", [&]() -> Coroutine<void> {
    Fiber& current = co_await GetCurrentFiber();

    auto server = current.Spawn("server", [&]() -> Coroutine<void> {
      co_await rc.Serve();
      server_exited = true;
      co_return;
    });

    auto client = current.Spawn("client", [&]() -> Coroutine<void> {
      co_await rc.Close();
      co_return;
    });

    co_await current.Join(server);
    co_await current.Join(client);
    co_return;
  });

  RunEventLoop(io);
  EXPECT_TRUE(server_exited);
}

// 7. Test case: Select Integration
TEST(RemoteCallTest, SelectIntegration) {
  boost::asio::io_context io;
  AsioExecutor executor(io);
  Manager manager(executor);

  RemoteCall rc;
  Event<void> event1;
  std::vector<std::string> sequence;

  manager.SpawnRoot("root", [&]() -> Coroutine<void> {
    Fiber& current = co_await GetCurrentFiber();

    auto select_loop = current.Spawn("select_loop", [&]() -> Coroutine<void> {
      bool loop_active = true;
      while (loop_active) {
        auto [rpcResult, event1Result] = co_await Select(SelectPair(rc.GetServiceAwaitor(), RemoteCall::HandleRequest),
                                                         SelectPair(event1, [&]() -> bool {
                                                           sequence.push_back("event1_fired");
                                                           return false;
                                                         }));
        if (rpcResult.has_value() && !rpcResult.value()) {
          loop_active = false;
        }
        if (event1Result.has_value() && !event1Result.value()) {
          loop_active = false;
        }
      }
      co_return;
    });

    auto client = current.Spawn("client", [&]() -> Coroutine<void> {
      auto reply = co_await rc.Call([]() -> Coroutine<std::string> { co_return "rpc_done"; });
      EXPECT_TRUE(reply.has_value());
      EXPECT_EQ(reply.value(), "rpc_done");
      sequence.push_back("client_rpc_returned");

      // Now fire event1 to exit the select loop
      event1.Fire();
      co_return;
    });

    co_await current.Join(select_loop);
    co_await current.Join(client);
    co_await rc.Close();
    co_return;
  });

  RunEventLoop(io);

  // Verify execution order
  ASSERT_EQ(sequence.size(), 2);
  EXPECT_EQ(sequence[0], "client_rpc_returned");
  EXPECT_EQ(sequence[1], "event1_fired");
}
