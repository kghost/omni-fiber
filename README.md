# OmniFiber

**OmniFiber** is a lightweight, high-performance cooperative multi-tasking (fiber) library for C++23. It seamlessly integrates C++ standard coroutines with the **Boost.Asio** asynchronous event loop, allowing developers to write highly concurrent I/O-bound applications in a clean, readable, synchronous style without callback hell, complex state machines, or thread-blocking overhead.

---

## Key Features

- **Native C++23 Coroutines**: Built on standard `std::coroutine_handle` and custom promise types. Supports C++23 explicit object parameters (`this Impl& self`).
- **Structured Concurrency**: Spawns fibers in a robust parent-child hierarchy. Enforces structured joining, allowing parents to wait for single, first, or all child fibers cooperatively, with seamless exception propagation.
- **Boost.Asio Integration**: Built-in `AsioExecutor` and specialized completion token `AsioUseFiber` to orchestrate fibers directly within a Boost.Asio event loop (`boost::asio::io_context`).
- **Cooperative Sync Primitives**: Awaitable synchronization tools (`Event<T>`, `Signal`, `Pipe<T>`, `EventQueue<T>`) that yield execution instead of blocking OS threads.
- **Cooperative Multiplexing**: `Select` multiplexer to await multiple events simultaneously with automatic RAII-based cancellation support.
- **Advanced Diagnostics (`#ifndef NDEBUG`)**: Callstack reconstruction via promise chaining and instruction pointer tracking, integrated with DWARF symbol resolution (`libdw`) for human-readable stack traces.

---

## API Summary & Interface

OmniFiber resides in the `Omni::Fiber` namespace. The main components are summarized below:

### 1. Coroutine Wrapper (`Coroutine<RetType>`)
A type representing a C++ coroutine. Functions returning `Coroutine<RetType>` are cooperatively awaitable using `co_await`.
- Supports both `void` and non-`void` return types.
- Destructor asserts that the coroutine is fully completed, catching unawaited coroutines or lifetime mismatches early.
- **Usage**:
  ```cpp
  #include <omnifiber/Coroutine.hpp>

  Omni::Fiber::Coroutine<int> CalculateValue() {
      co_return 42;
  }
  ```

### 2. Fiber (`Fiber`)
Represents an independent cooperative unit of execution.
- **Spawning child fibers**: 
  ```cpp
  #include <omnifiber/Fiber.hpp>

  std::shared_ptr<Fiber> child = parent->Spawn("child_name", []() -> Coroutine<void> {
      // Fiber execution logic
      co_return;
  });
  ```
- **Joining & Waiting**:
  - `co_await parent->Join(child)`: Waits for the specified child to finish and propagates its unhandled exceptions (wrapped in `FiberException`).
  - `co_await parent->WaitFor()`: Waits for the *first* child to finish and returns its `std::shared_ptr<Fiber>` pointer (or propagates its exception).
  - `co_await parent->WaitAll()`: Blocks until all active and completed child fibers are fully joined.
  - `co_await parent->Wait(until_callback)`: Yields the fiber until the boolean callback condition returns true.

> [!IMPORTANT]
> **FIBER LIFECYCLE & JOINING RULES**:
> 1. **Parent-Child Enforcements**: `parent->Join(child)` can only be called from the **exact parent fiber** that spawned the child. Attempting to join a child fiber from a different fiber family (e.g., a sibling or grandparent fiber) will result in an assertion failure: `Assertion _Children.contains(child) || _FinishedChildren.contains(child) failed`.
> 2. **Active Children Constraints**: A parent fiber must not exit/finish execution while it still has active or unjoined child fibers. Always ensure that parent fibers invoke `co_await parent->WaitAll()` (or join all children explicitly) before returning. Failure to do so will result in: `Assertion _Children.empty() && _FinishedChildren.empty() failed`.

### 3. Manager (`Manager`)
The fiber scheduler. It runs a queue of "ready" fibers and schedules them onto the underlying executor.
- **Initialization**:
  ```cpp
  #include <omnifiber/Manager.hpp>
  #include <omnifiber/Asio.hpp>
  
  Omni::Fiber::AsioExecutor executor(io_context);
  Omni::Fiber::Manager manager(executor);
  ```
- **Root Fiber**: Spawns the main outer fiber to start the execution tree.
  ```cpp
  std::shared_ptr<Fiber> root = manager.SpawnRoot("root", [&]() -> Coroutine<void> {
      // Main fiber loop
      co_return;
  });
  ```

### 4. GetCurrentOmniFiber Awaitable (`GetCurrentOmniFiber`)
An awaitable to retrieve a reference to the active `Fiber` from inside a running fiber, without relying on any global or thread-local states.
- **Usage**:
  ```cpp
  #include <omnifiber/GetCurrentOmniFiber.hpp>

  Omni::Fiber::Coroutine<void> Worker() {
      Fiber& current = co_await Omni::Fiber::GetCurrentOmniFiber();
      // Use current fiber reference...
      co_return;
  }
  ```

### 5. Boost.Asio Completion Token (`AsioUseFiber`)
The flagship integration feature. Pass `Omni::Fiber::AsioUseFiber` as the completion token to any Boost.Asio async operation (e.g., `async_read`, `async_write`, `async_wait`) to automatically suspend the running fiber and resume it once the operation completes.
- **Usage**:
  ```cpp
  #include <omnifiber/Asio.hpp>

  // Wait for 1 second cooperatively inside a fiber without blocking the thread
  boost::asio::steady_timer timer(io_context, std::chrono::seconds(1));
  std::tuple<boost::system::error_code> res = co_await timer.async_wait(Omni::Fiber::AsioUseFiber);
  ```

### 6. Synchronization & Multiplexing Primitives

#### `Event<DataType = void>`
A cooperative notification event that can optionally hold a data payload.
```cpp
#include <omnifiber/Event.hpp>

Omni::Fiber::Event<int> dataEvent;

// Inside Fiber A (Waiter)
int result = co_await dataEvent; // Suspends until fired, then returns 42

// Inside Fiber B (Notifier)
dataEvent.Fire(42);              // Wakes up all waiting fibers
```

#### `Signal`
A stateless, one-shot-and-forget notification primitive.
```cpp
#include <omnifiber/Signal.hpp>

Omni::Fiber::Signal signal;

// Inside Fiber A (Waiter)
co_await signal; // Suspends Fiber A. Subsequent awaits will also suspend.

// Inside Fiber B (Notifier)
signal.Fire();   // Wakes up all current waiters
```

#### `Pipe<DataType>`
A cooperatively-blocking synchronous channel with a capacity of 1 element.
```cpp
#include <omnifiber/Pipe.hpp>

Omni::Fiber::Pipe<std::string> pipe;
auto producer = pipe.GetProducer();
auto consumer = pipe.GetConsumer();

// Inside Producer Fiber
co_await producer.Put("Hello, Pipe!"); // Blocks until consumed
producer.Shutdown();                      // Closes the pipe

// Inside Consumer Fiber
std::expected<std::string, PipeClosed> res = co_await consumer; // Suspends until ready
if (res.has_value()) {
    std::cout << "Received: " << res.value() << std::endl;
}
```

#### `Mutex`
A cooperative mutual exclusion primitive for non-preemptive fiber coroutines. `co_await mutex.Wait()` yields execution until the lock is acquired, returning a RAII `std::unique_ptr<LockGuard>`.
```cpp
#include <omnifiber/Mutex.hpp>

Omni::Fiber::Mutex mutex;

// Inside Fiber A or B
{
    std::unique_ptr<Omni::Fiber::LockGuard> guard = co_await mutex.Wait();
    // Critical section guarded by LockGuard...
} // Automatically unlocks and schedules next waiting fiber on destruction
```

#### `Select`
Multiplexes multiple awaitables deriving from `AwaiterBase` (e.g., events, pipes).
```cpp
#include <omnifiber/Select.hpp>

Event<int> event1;
Pipe<std::string>::Consumer consumer = pipe.GetConsumer();

co_await Select(
    SelectPair(event1, [](int val) { std::cout << "Event: " << val << "\n"; }),
    SelectPair(consumer, [](auto res) { std::cout << "Pipe read occurred\n"; })
);
```

---

## Quick Start Example

Below is a complete example demonstrating how to initialize the `Manager`, bind it to Boost.Asio, and run cooperative fibers:

```cpp
#include <iostream>
#include <boost/asio.hpp>
#include <omnifiber/Coroutine.hpp>
#include <omnifiber/Fiber.hpp>
#include <omnifiber/GetCurrentOmniFiber.hpp>
#include <omnifiber/Manager.hpp>
#include <omnifiber/Asio.hpp>
#include <omnifiber/Event.hpp>

using namespace Omni::Fiber;

Coroutine<void> WorkerFiber(int id, std::shared_ptr<Event<void>> startSignal, boost::asio::any_io_executor executor) {
    std::cout << "[Worker " << id << "] Waiting for start signal..." << std::endl;
    co_await *startSignal; // OmniYield until signaled

    std::cout << "[Worker " << id << "] Started! Performing async work..." << std::endl;
    
    // Perform a cooperative sleep using Boost.Asio timer
    boost::asio::steady_timer timer(io, std::chrono::seconds(1));
    co_await timer.async_wait(AsioUseFiber);
    
    std::cout << "[Worker " << id << "] Done!" << std::endl;
}

int main() {
    boost::asio::io_context io;
    AsioExecutor executor(io.get_executor());
    Manager manager(executor);

    auto startSignal = std::make_shared<Event<void>>();

    manager.SpawnRoot("root", [&]() -> Coroutine<void> {
        Fiber& currentFiber = co_await GetCurrentOmniFiber();
        std::cout << "[Root] Spawning worker fibers..." << std::endl;

        auto worker1 = currentFiber.Spawn("worker1", [&]() { return WorkerFiber(1, startSignal, io); });
        auto worker2 = currentFiber.Spawn("worker2", [&]() { return WorkerFiber(2, startSignal, io); });

        std::cout << "[Root] Setting up..." << std::endl;
        
        // Signal the workers
        std::cout << "[Root] Signaling workers to start!" << std::endl;
        startSignal->Fire();

        // Join workers
        co_await currentFiber.Join(worker1);
        co_await currentFiber.Join(worker2);

        std::cout << "[Root] All workers finished!" << std::endl;
    });

    // Run the Boost.Asio event loop
    io.run();

    return 0;
}
```

---

## Building and Installation

OmniFiber can be built and linked via CMake:

```cmake
find_package(omnifiber REQUIREDConfig)
# Normal variant
target_link_libraries(your_target PRIVATE omnifiber::omnifiber)
# AddressSanitizer variant
target_link_libraries(your_target PRIVATE omnifiber::omnifiber-asan)
```

**Requirements**:
- C++23 compliant compiler (GCC 13+, Clang 16+, or MSVC 2022+).
- Boost (1.75+) containing Log, Thread, and Asio components.
- `libdw` (ELF/DWARF library, required in Debug/non-production builds for stack-trace symbol resolution).
