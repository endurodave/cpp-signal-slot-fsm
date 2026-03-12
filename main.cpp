/**
 * @file main.cpp
 * @brief Self-Test Engine Example Entry Point.
 *
 * @details
 * This example demonstrates a robust, multi-threaded architecture for a
 * "Self-Test Engine" using the DelegateMQ library.
 *
 * **System Architecture:**
 * - **SelfTestEngine (Master):** A Singleton State Machine running on its own
 * dedicated worker thread. It coordinates the execution of sub-tests.
 * - **Sub-Tests:** Independent State Machines (`CentrifugeTest`, `PressureTest`)
 * that inherit from a common `SelfTest` base class.
 * - **User Interface:** A simulated UI thread that receives status updates
 * asynchronously from the Engine.
 *
 * **Key DelegateMQ Features Demonstrated:**
 * 1. **Safe Signals (SignalPtr):**
 * - Uses `std::shared_ptr<SignalSafe>` to ensure thread-safety and proper
 * lifetime management.
 * - The Engine exposes `OnStatus` (static) and `OnCompleted` (instance) signals.
 *
 * 2. **RAII Connection Management:**
 * - Uses `dmq::ScopedConnection` to store connection handles.
 * - This guarantees that signals are automatically disconnected if the
 * connection object goes out of scope, preventing dangling pointer crashes.
 *
 * 3. **Asynchronous Marshaling:**
 * - `MakeDelegate(..., userInterfaceThread)` is used to transparently marshal
 * status callbacks from the Engine Thread to the User Interface Thread.
 * - This eliminates the need for manual locking or message queues in application code.
 *
 * 4. **Hierarchical Coordination:**
 * - The Engine subscribes to the `OnCompleted` / `OnFailed` signals of its
 * sub-tests to direct the overall test flow.
 *
 * @see https://github.com/endurodave/DelegateMQ
 */

#include "DelegateMQ.h"
#include "SelfTestEngine.h"
#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>

// @see https://github.com/endurodave/StateMachineWithModernDelegates
// David Lafreniere

using namespace std;
using namespace dmq;

// Flag to control the timer thread
std::atomic<bool> processTimerExit(false);

static void ProcessTimers()
{
    while (!processTimerExit.load())
    {
        // Process all delegate-based timers
        Timer::ProcessTimers();
        std::this_thread::sleep_for(std::chrono::microseconds(50));
    }
}

// A thread to capture self-test status callbacks for output to the "user interface"
Thread userInterfaceThread("UserInterface");

// Simple flag to exit main loop (Atomic for thread safety)
std::atomic<bool> selfTestEngineCompleted(false);

//------------------------------------------------------------------------------
// OnSelfTestEngineStatus
//------------------------------------------------------------------------------
void OnSelfTestEngineStatus(const SelfTestStatus& status)
{
    // Output status message to the console "user interface"
    cout << status.message.c_str() << endl;
}

//------------------------------------------------------------------------------
// OnSelfTestEngineComplete
//------------------------------------------------------------------------------
void OnSelfTestEngineComplete()
{
    selfTestEngineCompleted = true;
}

//------------------------------------------------------------------------------
// main
//------------------------------------------------------------------------------
int main(void)
{
    // Start the thread that will run ProcessTimers
    std::thread timerThread(ProcessTimers);

    // Create the worker threads
    userInterfaceThread.CreateThread();

    // Initialize instance
    SelfTestEngine::GetInstance();

    // -------------------------------------------------------------------------
    // CONNECT SIGNALS (RAII)
    // -------------------------------------------------------------------------
    // We must store the connection handles!
    // If we don't, ScopedConnection destructs immediately and disconnects.
    ScopedConnection statusConn;
    ScopedConnection completeConn;

    // Register for status updates (Static Signal)
    statusConn = SelfTestEngine::OnStatus.Connect(
        MakeDelegate(&OnSelfTestEngineStatus, userInterfaceThread)
    );

    // Register for completion (Instance Signal from base class SelfTest)
    completeConn = SelfTestEngine::GetInstance().OnCompleted.Connect(
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