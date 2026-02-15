![License MIT](https://img.shields.io/github/license/BehaviorTree/BehaviorTree.CPP?color=blue)
[![conan Ubuntu](https://github.com/endurodave/cpp-signal-slot-fsm/actions/workflows/cmake_ubuntu.yml/badge.svg)](https://github.com/endurodave/cpp-signal-slot-fsm/actions/workflows/cmake_ubuntu.yml)
[![conan Ubuntu](https://github.com/endurodave/cpp-signal-slot-fsm/actions/workflows/cmake_clang.yml/badge.svg)](https://github.com/endurodave/cpp-signal-slot-fsm/actions/workflows/cmake_clang.yml)
[![conan Windows](https://github.com/endurodave/cpp-signal-slot-fsm/actions/workflows/cmake_windows.yml/badge.svg)](https://github.com/endurodave/cpp-signal-slot-fsm/actions/workflows/cmake_windows.yml)

# C++ State Machine with Signal-Slots

A Finite State Machine (FSM) combining [C++ State Machine](https://github.com/endurodave/StateMachine) with the [DelegateMQ](https://github.com/endurodave/DelegateMQ) asynchronous Signal-Slot library.

# Table of Contents

- [C++ State Machine with Signal-Slots](#c-state-machine-with-signal-slots)
- [Table of Contents](#table-of-contents)
- [Introduction](#introduction)
- [Getting Started](#getting-started)
- [Asynchronous Signals](#asynchronous-signals)
- [Self-Test Subsystem](#self-test-subsystem)
  - [SelfTestEngine](#selftestengine)
  - [CentrifugeTest](#centrifugetest)
  - [Timer](#timer)
- [Poll Events](#poll-events)
- [User Interface](#user-interface)
- [Run-Time](#run-time)
- [References](#references)


# Introduction

A software-based Finite State Machines (FSM) is an implementation method used to decompose a design into states and events. Simple embedded devices with no operating system employ single threading such that the state machines run on a single thread. More complex systems use multithreading to divvy up the processing.

This repository combines state machines and asynchronous signal-slots into a single project. The goal for the article is to provide a complete working project with threads, timers, events, and state machines all working together. To illustrate the concept, the example project implements a state-based self-test engine utilizing asynchronous communication between threads.

Related GitHub repositories:

* [DelegateMQ in C++](https://github.com/endurodave/DelegateMQ) - a delegate library implementing asynchronous Signal-Slots. 
* [State Machine Design in C++](https://github.com/endurodave/StateMachine) - a compact C++ state machine.

# Getting Started
[CMake](https://cmake.org/) is used to create the project build files on any Windows or Linux machine.

1. Clone the repository.
2. From the repository root, run the following CMake command:   
   `cmake -B Build .`
3. Build and run the project within the `Build` directory. 

# Asynchronous Signals

If you're not familiar with a delegate, the concept is quite simple. A delegate can be thought of as a super function pointer. In C++, there's no single pointer type capable of pointing to all possible function variations: instance member, virtual, const, static, and free (global). A standard function pointer cannot point to instance member functions, and pointers to member functions have significant limitations. However, delegate classes can, in a type-safe way, point to any function provided the function signature matches. In short, a delegate points to any function with a matching signature to support anonymous function invocation.

Asynchronous delegates take this concept further by permitting anonymous invocation of any function on a client-specified thread of control. The function and all arguments are safely marshaled to and called from a destination thread, simplifying inter-thread communication and eliminating cross-threading errors.

The DelegateMQ library is used throughout this project to provide an effective publisher/subscriber mechanism using delegate-based **Signals**. A publisher exposes a signal (via `dmq::SignalPtr`) and subscribers **connect** delegates to that signal to receive anonymous asynchronous callbacks.

Key locations where signals are utilized:

* **Task Completion**: Within the `SelfTest` base class, the `OnCompleted` and `OnFailed` signals allow subscribers to connect delegates. Whenever a self-test completes, the signal is invoked to notify registered clients. For example, the `SelfTestEngine` connects to these signals in its sub-tests (`CentrifugeTest` and `PressureTest`) to be asynchronously informed of their progress.
* **Status Updates**: The user interface connects to the `SelfTestEngine::OnStatus` signal. This allows a client running on a separate thread (e.g., the `userInterfaceThread`) to receive status updates during execution. By using `MakeDelegate` with a destination thread, the developer can specify exactly where the callback executes, making it easy to avoid thread-safety issues in UI code.
* **Periodic Polling**: The `Timer` class utilizes a signal (`OnExpired`) to fire periodic callbacks to a registered function. This is particularly useful for event-driven state machines that need to poll for specific conditions. In this project, the `Timer` class injects periodic `Poll` events into state machine instances.

To ensure robust lifetime management, these connections are managed via **RAII** using `dmq::ScopedConnection`. Storing the connection handle guarantees that the subscriber is automatically disconnected if the handling object is destroyed, preventing "dangling pointer" crashes during asynchronous execution.



# Self-Test Subsystem

Self-tests execute a series of tests on hardware and mechanical systems to ensure correct operation. In this example, there are four state machine classes implementing our self-test subsystem as shown in the inheritance diagram below:

<p align="center"><img height="191" src="Figure_1.png" width="377" /></p>

<p align="center"><strong>Figure 1: Self-Test Subsystem Inheritance Diagram</strong></p>

## SelfTestEngine

`SelfTestEngine` is thread-safe and the main point of contact for client's utilizing the self-test subsystem. `CentrifugeTest` and `PressureTest` are members of SelfTestEngine. `SelfTestEngine` is responsible for sequencing the individual self-tests in the correct order as shown in the state diagram below.

<p align="center"><img height="370" src="Figure_2.png" width="530" /></p>

<p align="center"><strong>Figure 2: SelfTestEngine State Machine</strong></p>

The `Start` event initiates the self-test engine. `SelfTestEngine::Start()` is an asynchronous function that reinvokes the `Start()` function if the caller is not on the correct execution thread. Perform a simple check whether the caller is executing on the desired thread of control. If not, a temporary asynchronous delegate is created on the stack and then invoked. The delegate and all the caller's original function arguments are duplicated on the heap and the function is reinvoked on `m_thread`. This is an elegant way to create asynchronous API's with the absolute minimum of effort. Since `Start()` is asynchronous,  it is thread-safe to be called by any client running on any thread.

```cpp
void SelfTestEngine::Start(const StartData* data)
{
    // Is the caller executing on m_thread?
    if (m_thread.GetThreadId() != Thread::GetCurrentThreadId())
    {
        // Create an asynchronous delegate and reinvoke the function call on m_thread
        auto delegate = MakeDelegate(this, &SelfTestEngine::Start, m_thread);
        delegate(data);
        return;
    }

    BEGIN_TRANSITION_MAP                                    // - Current State -
        TRANSITION_MAP_ENTRY (ST_START_CENTRIFUGE_TEST)     // ST_IDLE
        TRANSITION_MAP_ENTRY (CANNOT_HAPPEN)                // ST_COMPLETED
        TRANSITION_MAP_ENTRY (CANNOT_HAPPEN)                // ST_FAILED
        TRANSITION_MAP_ENTRY (EVENT_IGNORED)                // ST_START_CENTRIFUGE_TEST
        TRANSITION_MAP_ENTRY (EVENT_IGNORED)                // ST_START_PRESSURE_TEST
    END_TRANSITION_MAP(data)
}
```

When each self-test completes, the `Complete` event fires causing the next self-test to start. After all of the tests are done, the state machine transitions to `Completed` and back to `Idle`. If the `Cancel` event is generated at any time during execution, a transition to the `Failed` state occurs.

The `SelfTest` base class provides three states common to all `SelfTest`-derived state machines: `Idle`, `Completed`, and `Failed`. `SelfTestEngine` then adds two more states: `StartCentrifugeTest `and `StartPressureTest`.

`SelfTestEngine` has one public event function, `Start()`, that starts the self-tests. `SelfTestEngine::OnStatus` is an asynchronous signal allowing client's to register for status updates during testing. A `Thread` instance is also contained within the class. All self-test state machine execution occurs on this thread.

```cpp
class SelfTestEngine : public SelfTest
{
public:
    // Clients register for asynchronous self-test status callbacks
    static inline dmq::SignalPtr<void(const SelfTestStatus&)> OnStatus =
        dmq::MakeSignal<void(const SelfTestStatus&)>();

    // Singleton instance of SelfTestEngine
    static SelfTestEngine& GetInstance();

    // Start the self-tests. This is a thread-safe asynchronous function. 
    void Start(const StartData* data);

    Thread& GetThread() { return m_thread; }
    static void InvokeStatusSignal(std::string msg);

private:
    SelfTestEngine();
    void Complete();

    // Sub self-test state machines 
    CentrifugeTest m_centrifugeTest;
    PressureTest m_pressureTest;

    // Worker thread used by all self-tests
    Thread m_thread;

    StartData m_startData;

    // RAII CONNECTIONS
    // Stores the handles to the signal connections. 
    // If these are destroyed, the engine stops listening to the sub-tests.
    dmq::ScopedConnection m_centrifugeCompleteConn;
    dmq::ScopedConnection m_centrifugeFailedConn;
    dmq::ScopedConnection m_pressureCompleteConn;
    dmq::ScopedConnection m_pressureFailedConn;

    // State enumeration order must match the order of state method entries
    // in the state map.
    enum States
    {
        ST_START_CENTRIFUGE_TEST = SelfTest::ST_MAX_STATES,
        ST_START_PRESSURE_TEST,
        ST_MAX_STATES
    };

    // Define the state machine state functions with event data type
    STATE_DECLARE(SelfTestEngine, StartCentrifugeTest, StartData)
    STATE_DECLARE(SelfTestEngine, StartPressureTest, NoEventData)

    // State map to define state object order. Each state map entry defines a
    // state object.
    BEGIN_STATE_MAP
        STATE_MAP_ENTRY(&Idle)
        STATE_MAP_ENTRY(&Completed)
        STATE_MAP_ENTRY(&Failed)
        STATE_MAP_ENTRY(&StartCentrifugeTest)
        STATE_MAP_ENTRY(&StartPressureTest)
    END_STATE_MAP
};
```

As mentioned previously, the `SelfTestEngine` registers for asynchronous signals from each sub self-tests (i.e. `CentrifugeTest` and `PressureTest`) as shown below. When a sub self-test state machine completes, the `SelfTestEngine::Complete()` function is called. When a sub self-test state machine fails, the `SelfTestEngine::Cancel()` function is called.

```cpp
SelfTestEngine::SelfTestEngine() :
    SelfTest(ST_MAX_STATES),
    m_thread("SelfTestEngine")
{
    // Important: Start the thread so it can process delegates
    m_thread.CreateThread();

    // Register for signals when sub self-test state machines complete or fail.
    // We MUST store the returned connection object, otherwise it will 
    // fall out of scope and disconnect immediately.
    m_centrifugeCompleteConn = m_centrifugeTest.OnCompleted->Connect(
        MakeDelegate(this, &SelfTestEngine::Complete, m_thread)
    );
    
    m_centrifugeFailedConn = m_centrifugeTest.OnFailed->Connect(
        MakeDelegate<SelfTest>(this, &SelfTest::Cancel, m_thread)
    );

    m_pressureCompleteConn = m_pressureTest.OnCompleted->Connect(
        MakeDelegate(this, &SelfTestEngine::Complete, m_thread)
    );

    m_pressureFailedConn = m_pressureTest.OnFailed->Connect(
        MakeDelegate<SelfTest>(this, &SelfTest::Cancel, m_thread)
    );
}
```

The `SelfTest` base class generates the `OnCompleted` and `OnFailed` within the `Completed` and `Failed` states respectively as seen below:

```cpp
STATE_DEFINE(SelfTest, Completed, NoEventData)
{
    SelfTestEngine::InvokeStatusSignal("SelfTest::ST_Completed");

    if (OnCompleted)
        OnCompleted();

    InternalEvent(ST_IDLE);
}

STATE_DEFINE(SelfTest, Failed, NoEventData)
{
    SelfTestEngine::InvokeStatusSignal("SelfTest::ST_Failed");

    if (OnFailed)
        OnFailed();

    InternalEvent(ST_IDLE);
}
```

One might ask why the state machines use asynchronous delegate signals. If the state machines are on the same thread, why not use a normal, synchronous callback instead? The problem to prevent is a callback into a currently executing state machine, that is, the call stack wrapping back around into the same class instance. For example, the following call sequence should be prevented: `SelfTestEngine` calls `CentrifugeTest` calls back `SelfTestEngine`. An asynchronous callback allows the stack to unwind and prevents this unwanted behavior.

## CentrifugeTest

The `CentrifugeTest` state machine diagram shown below implements the centrifuge self-test described in "<a href="https://github.com/endurodave/StateMachine"><strong>State Machine Design in C++</strong></a>". `CentrifugeTest` uses state machine inheritance by inheriting the `Idle`, `Completed` and `Failed` states from the `SelfTest` class. The difference here is that the `Timer` class is used to provide `Poll `events via asynchronous delegate signal.

<p align="center"><img height="765" src="CentrifugeTest.png" width="520" /></p>

<p align="center"><strong>Figure 3: CentrifugeTest State Machine</strong></p>

## Timer

The `Timer` class provides a common mechanism to receive signal callbacks by registering with `OnExpired`. `Start()` starts the callbacks at a particular interval. `Stop()` stops the callbacks.

```cpp
/// @brief A timer class provides periodic timer callbacks on the client's 
/// thread of control. Timer is thread safe.
class Timer
{
public:
    /// Client's register with OnExpired to get timer callbacks
    dmq::SignalPtr<void(void)> OnExpired;

    /// Constructor
    Timer(void);

    /// Destructor
    ~Timer(void);

    /// Starts a timer for callbacks on the specified timeout interval.
    /// @param[in] timeout - the timeout.
    /// @param[in] once - true if only one timer expiration
    void Start(dmq::Duration timeout, bool once = false);

    /// Stops a timer.
    void Stop();
...
```

# Poll Events

`CentrifugeTest` has a `Timer` instance and registers for signals. The signal callback function, a thread instance and a `this` pointer is provided to `Connect()` facilitating the asynchronous signal mechanism.

```cpp
// Register for timer callbacks
m_pollTimerConn = m_pollTimer.OnExpired->Connect(
    MakeDelegate(this, &CentrifugeTest::Poll, SelfTestEngine::GetInstance().GetThread())
);
```

When the timer is started using `Start()`, the `Poll()` event function is periodically called at the interval specified. Notice that when the `Poll()` external event function is called, a transition to either `WaitForAcceleration` or `WaitForDeceleration` is performed based on the current state of the state machine. If `Poll()` is called at the wrong time, the event is silently ignored.

```cpp
void CentrifugeTest::Poll()
{
    BEGIN_TRANSITION_MAP                                   // - Current State -
        TRANSITION_MAP_ENTRY (EVENT_IGNORED)               // ST_IDLE
        TRANSITION_MAP_ENTRY (EVENT_IGNORED)               // ST_COMPLETED
        TRANSITION_MAP_ENTRY (EVENT_IGNORED)               // ST_FAILED
        TRANSITION_MAP_ENTRY (EVENT_IGNORED)               // ST_START_TEST
        TRANSITION_MAP_ENTRY (ST_WAIT_FOR_ACCELERATION)    // ST_ACCELERATION
        TRANSITION_MAP_ENTRY (ST_WAIT_FOR_ACCELERATION)    // ST_WAIT_FOR_ACCELERATION
        TRANSITION_MAP_ENTRY (ST_WAIT_FOR_DECELERATION)    // ST_DECELERATION
        TRANSITION_MAP_ENTRY (ST_WAIT_FOR_DECELERATION)    // ST_WAIT_FOR_DECELERATION
    END_TRANSITION_MAP(NULL)
}

STATE_DEFINE(CentrifugeTest, Acceleration, NoEventData)
{
    SelfTestEngine::InvokeStatusSignal("CentrifugeTest::ST_Acceleration");

    // Start polling while waiting for centrifuge to ramp up to speed
    m_pollTimer.Start(10);
}
```

# User Interface

The project doesn't have a user interface except the text console output. For this example, the "user interface" just outputs self-test status messages on the user interface thread via the `OnSelfTestEngineStatus()` function:

```cpp
Thread userInterfaceThread("UserInterface");

void OnSelfTestEngineStatus(const SelfTestStatus& status)
{
    // Output status message to the console "user interface"
    cout << status.message.c_str() << endl;
}
```

Before the self-test starts, the user interface registers with the `SelfTestEngine::OnStatus` signal.

```cpp
    statusConn = SelfTestEngine::OnStatus->Connect(
        MakeDelegate(&OnSelfTestEngineStatus, userInterfaceThread)
    );
```

The user interface thread here is just used to simulate signals to a GUI library normally running in a separate thread of control.

# Run-Time

The program's `main()` function is shown below. It creates the two threads, registers for signals from `SelfTestEngine`, then calls `Start()` to start the self-tests.

```cpp
int main(void)
{
    // Start the thread that will run ProcessTimers
    std::thread timerThread(ProcessTimers);

    // Create the worker threads
    userInterfaceThread.CreateThread();

    // Note: SelfTestEngine starts its thread in its constructor now,
    // but calling CreateThread() again is harmless (idempotent).
    SelfTestEngine::GetInstance().GetThread().CreateThread();

    // -------------------------------------------------------------------------
    // CONNECT SIGNALS (RAII)
    // -------------------------------------------------------------------------
    // We must store the connection handles!
    // If we don't, ScopedConnection destructs immediately and disconnects.
    ScopedConnection statusConn;
    ScopedConnection completeConn;

    // Register for status updates (Static Signal)
    statusConn = SelfTestEngine::OnStatus->Connect(
        MakeDelegate(&OnSelfTestEngineStatus, userInterfaceThread)
    );

    // Register for completion (Instance Signal from base class SelfTest)
    completeConn = SelfTestEngine::GetInstance().OnCompleted->Connect(
        MakeDelegate(&OnSelfTestEngineComplete, userInterfaceThread)
    );

    // Start self-test engine
    StartData startData;
    startData.shortSelfTest = TRUE;
    SelfTestEngine::GetInstance().Start(&startData);

    // Wait for self-test engine to complete
    while (!selfTestEngineCompleted)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // -------------------------------------------------------------------------
    // DISCONNECT
    // -------------------------------------------------------------------------
    // Explicitly disconnect (optional, as destructors would handle this automatically)
    statusConn.Disconnect();
    completeConn.Disconnect();

    // Exit the worker threads
    userInterfaceThread.ExitThread();
    SelfTestEngine::GetInstance().GetThread().ExitThread();

    // Ensure the timer thread completes before main exits
    processTimerExit.store(true);
    if (timerThread.joinable())
        timerThread.join();

    return 0;
}
```

`SelfTestEngine` generates asynchronous signals on the `UserInteface` thread. The `OnSelfTestEngineStatus()` signal callback outputs the message to the console.

```cpp
void OnSelfTestEngineStatus(const SelfTestStatus& status)
{
      // Output status message to the console "user interface"
      cout << status.message.c_str() << endl;
}
```

The `OnSelfTestEngineComplete()` callback sets a flag to let the `main()` loop exit.

```cpp
void OnSelfTestEngineComplete()
{
      selfTestEngineCompleted = true;
}
```

Running the project outputs the following console messages:

<p align="left"><img height="385" src="Figure_4.png" width="628" /></p>

<p align="left"><strong>Figure 4: Console Output</strong></p>

# References

* [**State Machine Design in C++**](https://github.com/endurodave/StateMachine) - A compact C++ finite state machine implementation supporting internal and external events.
* [**DelegateMQ**](https://github.com/endurodave/DelegateMQ) - A modern C++ messaging library for synchronous and asynchronous function invocation across threads.
* [**C++ State Machine with Threads**](https://github.com/endurodave/StateMachineWithThreads) – An example project demonstrating state machine integration with multiple worker threads.
* [**C++ std::thread Event Loop with Message Queue and Timer**](https://github.com/endurodave/StdWorkerThread) - A lightweight thread-safe event loop implementation using standard C++ threads and message queues.




