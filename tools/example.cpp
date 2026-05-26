#include <boost/asio.hpp>
#include <chrono>
#include <iostream>

#include "Asio.hpp"
#include "Coroutine.hpp"
#include "EventQueue.hpp"
#include "Fiber.hpp"
#include "GetCurrentFiber.hpp"
#include "Manager.hpp"

using namespace Omni::Fiber;

// A cooperative worker fiber that processes tasks from a shared queue.
// It uses a "poison pill" pattern (task == -1) to know when to shut down.
Coroutine<void> WorkerFiber(int id, boost::asio::io_context& io, std::shared_ptr<EventQueue<int>> taskQueue) {
  std::cout << "[Worker " << id << "] Fiber started. Awaiting tasks..." << std::endl;

  while (true) {
    // Wait cooperatively for a task to arrive.
    // Since EventQueue co_await yields control when empty, this worker consumes 0% CPU while idle.
    co_await *taskQueue;

    // Since multiple worker fibers are co_awaiting the same queue, all of them are
    // resumed when a task is pushed. However, the first worker to resume will consume
    // the task. The others must check if the queue is empty and go back to sleep.
    if (taskQueue->IsEmpty()) {
      continue;
    }

    int task = taskQueue->PopFront();
    if (task == -1) {
      // Received poison pill (shutdown signal)
      break;
    }

    std::cout << "[Worker " << id << "] Processing task: " << task << std::endl;

    // Simulate some cooperative task processing time using Boost.Asio timers.
    // This yields execution of the current fiber back to the event loop,
    // allowing other fibers to run on the same thread without blocking!
    boost::asio::steady_timer timer(io, std::chrono::milliseconds(100));
    co_await timer.async_wait(AsioUseFiber);
  }

  std::cout << "[Worker " << id << "] Fiber completed gracefully." << std::endl;
  co_return;
}

// A cooperative producer fiber that generates tasks at intervals
Coroutine<void> ProducerFiber(boost::asio::io_context& io, std::shared_ptr<EventQueue<int>> taskQueue, int numWorkers) {
  std::cout << "[Producer] Fiber started. Generating tasks..." << std::endl;

  for (int i = 1; i <= 5; ++i) {
    // Cooperative sleep between generating tasks
    boost::asio::steady_timer timer(io, std::chrono::milliseconds(150));
    co_await timer.async_wait(AsioUseFiber);

    std::cout << "[Producer] Pushing task: " << i * 100 << std::endl;
    taskQueue->Push(i * 100);
  }

  std::cout << "[Producer] All tasks generated. Pushing shutdown signals..." << std::endl;
  for (int i = 0; i < numWorkers; ++i) {
    taskQueue->Push(-1); // Send a poison pill to each worker
  }

  co_return;
}

int main() {
  std::cout << "=== OmniFiber Cooperative Task Queue Example ===" << std::endl;

  // Create the Boost.Asio event loop
  boost::asio::io_context io;

  // Create the OmniFiber Asio Executor and Manager
  AsioExecutor executor(io);
  Manager manager(executor);

  // Shared communication primitives
  auto taskQueue = std::make_shared<EventQueue<int>>();
  const int numWorkers = 3;

  // Spawn the root fiber to orchestrate the worker and producer tree
  manager.SpawnRoot("root", [&]() -> Coroutine<void> {
    Fiber& current = co_await GetCurrentFiber();
    std::cout << "[Root] Spawning fibers..." << std::endl;

    // Spawn three worker fibers
    auto worker1 = current.Spawn("worker1", [&]() { return WorkerFiber(1, io, taskQueue); });
    auto worker2 = current.Spawn("worker2", [&]() { return WorkerFiber(2, io, taskQueue); });
    auto worker3 = current.Spawn("worker3", [&]() { return WorkerFiber(3, io, taskQueue); });

    // Spawn one producer fiber
    auto producer = current.Spawn("producer", [&]() { return ProducerFiber(io, taskQueue, numWorkers); });

    // Wait cooperatively for the producer to finish generating all tasks
    co_await current.Join(producer);
    std::cout << "[Root] Producer has finished. Waiting for workers to complete..." << std::endl;

    // Wait cooperatively for all workers to finish their processing
    co_await current.Join(worker1);
    co_await current.Join(worker2);
    co_await current.Join(worker3);

    std::cout << "[Root] All workers finished. Example complete!" << std::endl;
    co_return;
  });

  // Run the event loop. The single-threaded loop will drive all fibers cooperatively.
  io.run();

  std::cout << "================================================" << std::endl;
  return 0;
}
