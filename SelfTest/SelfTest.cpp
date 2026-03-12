#include "SelfTest.h"
#include "SelfTestEngine.h"
#include "DelegateMQ.h"

//------------------------------------------------------------------------------
// SelfTest
//------------------------------------------------------------------------------
SelfTest::SelfTest(INT maxStates) :
    StateMachine(maxStates)
{
}

//------------------------------------------------------------------------------
// Cancel
//------------------------------------------------------------------------------
void SelfTest::Cancel()
{
    // State machine base classes can't use a transition map, only the 
    // most-derived state machine class within the hierarchy can. So external 
    // events like this use the current state and call ExternalEvent()
    // to invoke the state machine transition. 
    if (GetCurrentState() != ST_IDLE)
        ExternalEvent(ST_FAILED);
}

//------------------------------------------------------------------------------
// Idle
//------------------------------------------------------------------------------
STATE_DEFINE(SelfTest, Idle, NoEventData)
{
    SelfTestEngine::InvokeStatusSignal("SelfTest::ST_Idle");
}

//------------------------------------------------------------------------------
// EntryIdle
//------------------------------------------------------------------------------
ENTRY_DEFINE(SelfTest, EntryIdle, NoEventData)
{
    SelfTestEngine::InvokeStatusSignal("SelfTest::EN_EntryIdle");
}

//------------------------------------------------------------------------------
// Completed
//------------------------------------------------------------------------------
STATE_DEFINE(SelfTest, Completed, NoEventData)
{
    SelfTestEngine::InvokeStatusSignal("SelfTest::ST_Completed");

    OnCompleted();

    InternalEvent(ST_IDLE);
}

//------------------------------------------------------------------------------
// Failed
//------------------------------------------------------------------------------
STATE_DEFINE(SelfTest, Failed, NoEventData)
{
    SelfTestEngine::InvokeStatusSignal("SelfTest::ST_Failed");

    OnFailed();

    InternalEvent(ST_IDLE);
}