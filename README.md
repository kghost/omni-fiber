# OmniFiber

**OmniFiber** is a lightweight, high-performance cooperative multi-tasking (fiber) library for C++20. It seamlessly integrates C++20 stackless coroutines with the **Boost.Asio** asynchronous event loop, allowing developers to write highly concurrent I/O-bound applications in a clean, readable, synchronous style without callback hell or complex state machines.

---

## Key Features

- **Native C++20 Coroutines**: Built on standard `std::coroutine_handle` and custom promise types.
- **Structured Concurrency**: Spawns fibers in a parent-child hierarchy, allowing parents to track and `Join` child fibers cooperatively.
- **Pluggable Executors**: Decoupled scheduler logic running on an abstract `Executor` interface.
- **Boost.Asio Integration**: Built-in `AsioExecutor` and custom completion token `AsioUseFiber` to orchestrate fibers directly within a Boost.Asio event loop (`boost::asio::io_context`).
- **Cooperative Sync Primitives**: Awaitable synchronization tools like `Event` and `EventQueue<T>` that yield execution instead of blocking threads.
- **Graceful Interruption**: Cooperatively interrupt running fibers with exception propagation (`FiberInterrupted`).

---

## API Summary & Interface

OmniFiber resides in the `Omni::Fiber` namespace. The main components are summarized below:

### 1. Coroutine Wrapper (`Coroutine<RetType>`)
A type representing a C++20 coroutine. Functions returning `Coroutine<RetType>` are cooperatively awaitable using `co_await`.
- Supports both `void` and non-`void` return types.
- Handles return values and exceptions automatically.
- **Usage**:
  ```cpp
  #include <omnifiber/Coroutine.h>

  Omni::Fiber::Coroutine<int> CalculateValue() {
      co_return 42;
  }
  ```

### 2. Fiber (`Fiber`)
Represents an independent cooperative thread of execution.
- **Spawning child fibers**: 
  ```cpp
  std::shared_ptr<Fiber> child = parent->Spawn("child_name", []() -> Coroutine<void> {
      // Fiber execution logic
      co_return;
  });
  ```
- **Joining**: A parent fiber can wait for a child fiber using `Join`:
  ```cpp
  co_await parent->Join(child);
  ```
- **Interruption**: Forcefully mark a fiber as interrupted.
  ```cpp
  child->Interrupt(); // Throws FiberInterrupted upon next suspension/resume
  ```

### 3. Manager (`Manager`)
The fiber scheduler. It runs a queue of "ready" fibers and schedules them onto the underlying executor.
- **Initialization**:
  ```cpp
  #include <omnifiber/ManagerDeclare.h>
  #include <omnifiber/ManagerDefine.h>
  
  Omni::Fiber::AsioExecutor executor(io_context);
  Omni::Fiber::Manager manager(executor);
  ```
- **Root Fiber**: Spawns the main outer fiber to start the execution tree.
  ```cpp
  manager.SpawnRoot("root", [&]() -> Coroutine<void> {
      // Main fiber loop
      co_return;
  });
  ```

### 4. Boost.Asio Completion Token (`AsioUseFiber`)
The flagship integration feature. Pass `Omni::Fiber::AsioUseFiber` as the completion token to any Boost.Asio async operation (e.g., `async_read`, `async_write`, `async_wait`) to automatically suspend the running fiber and resume it once the operation completes.
- **Usage**:
  ```cpp
  #include <omnifiber/Asio.h>

  // Wait for 1 second cooperatively inside a fiber without blocking the thread
  boost::asio::steady_timer timer(io_context, std::chrono::seconds(1));
  std::tuple<boost::system::error_code> res = co_await timer.async_wait(Omni::Fiber::AsioUseFiber);
  ```

### 5. Cooperative Synchronization Primitives

#### `Event`
A cooperative notification event. A fiber can wait on the event, suspending itself until another fiber triggers `Set()`.
```cpp
#include <omnifiber/Event.h>

Omni::Fiber::Event evt;

// Inside Fiber A (Waiter)
co_await evt; // Suspends fiber A

// Inside Fiber B (Notifier)
evt.Set();    // Wakes up Fiber A (schedules it for execution)
```

#### `EventQueue<Element>`
A thread-safe, cooperatively awaitable queue. Perfect for producer-consumer pipelines between fibers.
```cpp
#include <omnifiber/EventQueue.h>

Omni::Fiber::EventQueue<std::string> msgQueue;

// Inside Consumer Fiber
co_await msgQueue; // Suspends until queue is non-empty
std::string msg = msgQueue.PopFront();

// Inside Producer Fiber
msgQueue.Push("Hello, Fiber!"); // Triggers event, resumes consumer
```

---

## Quick Start Example

Below is a complete, working-style example demonstrating how to initialize the `Manager`, bind it to Boost.Asio, and run cooperative fibers:

```cpp
#include <iostream>
#include <boost/asio.hpp>
#include <omnifiber/Coroutine.h>
#include <omnifiber/Fiber.h>
#include <omnifiber/ManagerDeclare.h>
#include <omnifiber/ManagerDefine.h>
#include <omnifiber/Asio.h>
#include <omnifiber/Event.h>

using namespace Omni::Fiber;

Coroutine<void> WorkerFiber(int id, std::shared_ptr<Event> startSignal) {
    std::cout << "[Worker " << id << "] Waiting for start signal..." << std::endl;
    co_await *startSignal; // Cooperatively yield until signaled

    std::cout << "[Worker " << id << "] Started! Performing async work..." << std::endl;
    
    // Perform a cooperative sleep using Boost.Asio timer
    auto& io = boost::asio::query(boost::asio::system_executor(), boost::asio::execution::context); // or pass io_context
    boost::asio::steady_timer timer(Manager::GetCurrentFiber()->_Manager.GetRunner()._Manager...); // Simplified context usage
    // Real code usually passes io_context down or captures it.
}

int main() {
    boost::asio::io_context io;
    AsioExecutor executor(io);
    Manager manager(executor);

    auto startSignal = std::make_shared<Event>();

    manager.SpawnRoot("root", [&]() -> Coroutine<void> {
        auto currentFiber = Manager::GetCurrentFiber();
        std::cout << "[Root] Spawning worker fibers..." << std::endl;

        auto worker1 = currentFiber->Spawn("worker1", [&]() { return WorkerFiber(1, startSignal); });
        auto worker2 = currentFiber->Spawn("worker2", [&]() { return WorkerFiber(2, startSignal); });

        // Simulate some setup delay
        std::cout << "[Root] Setting up..." << std::endl;
        
        // Signal the workers
        std::cout << "[Root] Signaling workers to start!" << std::endl;
        startSignal->Set();

        // Join workers
        co_await currentFiber->Join(worker1);
        co_await currentFiber->Join(worker2);

        std::cout << "[Root] All workers finished!" << std::endl;
    });

    // Run the Boost.Asio event loop. It will execute the posted fiber tasks.
    io.run();

    return 0;
}
```

---

## Building and Installation

OmniFiber is packaged as a shared library. Include it in your CMake project as follows:

```cmake
find_package(omnifiber REQUIREDConfig)
target_link_libraries(your_target PRIVATE omnifiber::omnifiber)
```

**Requirements**:
- C++20 compliant compiler (GCC 10+, Clang 11+, or MSVC 2019+).
- Boost (1.75+) containing log, thread, and asio components.
