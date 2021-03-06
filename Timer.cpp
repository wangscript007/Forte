#include "Clock.h"
#include "FTrace.h"
#include "Timer.h"
#include "RunLoop.h"
#include <boost/shared_ptr.hpp>
using namespace Forte;
using namespace boost;

Timer::Timer(const Forte::FString& name,
             const boost::shared_ptr<RunLoop> &runloop,
             const Callback &callback,
             const Timespec &interval,
             bool repeats) :
    mName(name),
    mRunLoop(runloop),
    mCallback(callback),
    mInterval(interval),
    mRepeats(repeats)
{
    FTRACE;
    if (!runloop) throw ETimerRunLoopInvalid();
}

Timer::~Timer()
{
    FTRACE;
}

