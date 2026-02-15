#include "SelfTestEngine.h"

using namespace dmq;

//------------------------------------------------------------------------------
// GetInstance
//------------------------------------------------------------------------------
SelfTestEngine& SelfTestEngine::GetInstance()
{
    static SelfTestEngine instance;
    return instance;
}

//------------------------------------------------------------------------------
// SelfTestEngine
//------------------------------------------------------------------------------
SelfTestEngine::SelfTestEngine() :
    SelfTest(ST_MAX_STATES),
    m_thread("SelfTestEngine")
{
    // Important: Start the thread so it can process delegates
    m_thread.CreateThread();

    // Register for signals when sub self-test state machines complete or fail.
    // We MUST store the returned connection object, otherwise it will 
    // fall out of scope and disconnect immediately.
    m_centrifugeCompleteConn = m_centrifugeTest.OnCompleted->Connect(MakeDelegate(this, &SelfTestEngine::Complete, m_thread));
    m_centrifugeFailedConn = m_centrifugeTest.OnFailed->Connect(MakeDelegate<SelfTest>(this, &SelfTest::Cancel, m_thread));

    m_pressureCompleteConn = m_pressureTest.OnCompleted->Connect(MakeDelegate(this, &SelfTestEngine::Complete, m_thread));
    m_pressureFailedConn = m_pressureTest.OnFailed->Connect(MakeDelegate<SelfTest>(this, &SelfTest::Cancel, m_thread));
}

//------------------------------------------------------------------------------
// InvokeStatusSignal
//------------------------------------------------------------------------------
void SelfTestEngine::InvokeStatusSignal(std::string msg)
{
    // Client(s) registered?
    if (OnStatus)
    {
        SelfTestStatus status;
        status.message = msg;

        // Callback registered client(s) via dereference
        (*OnStatus)(status);
    }
}

//------------------------------------------------------------------------------
// Start
//------------------------------------------------------------------------------
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
        TRANSITION_MAP_ENTRY(ST_START_CENTRIFUGE_TEST)      // ST_IDLE
        TRANSITION_MAP_ENTRY(CANNOT_HAPPEN)                 // ST_COMPLETED
        TRANSITION_MAP_ENTRY(CANNOT_HAPPEN)                 // ST_FAILED
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)                 // ST_START_CENTRIFUGE_TEST
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)                 // ST_START_PRESSURE_TEST
    END_TRANSITION_MAP(data)
}

//------------------------------------------------------------------------------
// Complete
//------------------------------------------------------------------------------
void SelfTestEngine::Complete()
{
    BEGIN_TRANSITION_MAP                                    // - Current State -
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)                 // ST_IDLE
        TRANSITION_MAP_ENTRY(CANNOT_HAPPEN)                 // ST_COMPLETED
        TRANSITION_MAP_ENTRY(CANNOT_HAPPEN)                 // ST_FAILED
        TRANSITION_MAP_ENTRY(ST_START_PRESSURE_TEST)        // ST_START_CENTRIFUGE_TEST
        TRANSITION_MAP_ENTRY(ST_COMPLETED)                  // ST_START_PRESSURE_TEST
    END_TRANSITION_MAP(NULL)
}

//------------------------------------------------------------------------------
// StartCentrifugeTest
//------------------------------------------------------------------------------
STATE_DEFINE(SelfTestEngine, StartCentrifugeTest, StartData)
{
    m_startData = *data;

    InvokeStatusSignal("SelfTestEngine::ST_CentrifugeTest");
    m_centrifugeTest.Start(&m_startData);
}

//------------------------------------------------------------------------------
// StartPressureTest
//------------------------------------------------------------------------------
STATE_DEFINE(SelfTestEngine, StartPressureTest, NoEventData)
{
    InvokeStatusSignal("SelfTestEngine::ST_PressureTest");
    m_pressureTest.Start(&m_startData);
}