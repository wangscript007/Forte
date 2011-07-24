#ifndef __Forte_Future_h__
#define __Forte_Future_h__

#include "AutoMutex.h"
#include "Exception.h"
#include "FTrace.h"
#include "ThreadCondition.h"
#include "Clock.h"
#include "Util.h"
#include <boost/exception_ptr.hpp>

namespace Forte
{
    EXCEPTION_CLASS(EFuture);
    EXCEPTION_SUBCLASS2(EFuture, EFutureResultAlreadySet,
                        "Future result is already set");
    EXCEPTION_SUBCLASS2(EFuture, EFutureExceptionUnknown,
                        "An unknown exception was thrown during async invocation");
    EXCEPTION_SUBCLASS2(EFuture, EFutureTimeoutWaitingForResult,
                        "Timed out waiting for result to be set");

    template<typename ResultType>
    class Future : public Forte::Object
    {
        friend class AsyncInvocation;
    public:
        Future() : mCondition(mLock), mCancelled(false), mResultReady(false) {};
        virtual ~Future() {};

        virtual void Cancel() { AutoUnlockMutex lock(mLock); mCancelled = true; }
        virtual bool IsCancelled() const { AutoUnlockMutex lock(mLock); return mCancelled; }
        virtual bool IsReady() const { return mResultReady; }

        virtual ResultType GetResultTimed(const Timespec &timeout) {
            AutoUnlockMutex lock(mLock);
            if (!mResultReady)
            {
                if (timeout.IsZero())
                {
                    throw EFutureTimeoutWaitingForResult();
                }
                else if (timeout.IsPositive())
                {
                    MonotonicClock mtc;
                    RealtimeClock rtc;
                    Timespec mtTimeout = mtc.GetTime() + timeout;
                    while (!mResultReady && (mtc.GetTime() <= mtTimeout))
                    {
                        Timespec rtTimeout = rtc.GetTime()
                                + (mtTimeout - mtc.GetTime());
                        mCondition.TimedWait(rtTimeout);
                    }
                    if (!mResultReady)
                        throw EFutureTimeoutWaitingForResult();
                }
                else
                {
                    mCondition.Wait();
                }
            }
            if (mException)
            {
                try
                {
                    boost::rethrow_exception(mException);
                }
                catch (boost::unknown_exception &e)
                {
                    throw EFutureExceptionUnknown();
                }
            }
            return mResult;
        }

        virtual ResultType GetResult() {
            return GetResultTimed(Timespec::FromSeconds(-1));
        }
        
        virtual void SetResult(const ResultType &result) {
            AutoUnlockMutex lock(mLock);
            if (mResultReady)
                throw EFutureResultAlreadySet();
            mResult = result;
        }
        virtual void SetException(boost::exception_ptr e) {
            AutoUnlockMutex lock(mLock);
            mException = e;
            mResultReady = true;
            mCondition.Broadcast();
        }
    private:
        mutable Forte::Mutex mLock;
        Forte::ThreadCondition mCondition;
        bool mCancelled;
        bool mResultReady;
        ResultType mResult;
        boost::exception_ptr mException;
    };

    template<>
    class Future<void> : public Forte::Object
    {
        friend class AsyncInvocation;
    public:
        Future() : mCondition(mLock), mCancelled(false), mResultReady(false) {};
        virtual ~Future() {};

        virtual void Cancel() { AutoUnlockMutex lock(mLock); mCancelled = true; }
        virtual bool IsCancelled() const { AutoUnlockMutex lock(mLock); return mCancelled; }
        virtual bool IsReady() const { return mResultReady; }

        virtual void GetResultTimed(const Timespec &timeout) {
            AutoUnlockMutex lock(mLock);
            if (!mResultReady)
            {
                if (timeout.IsZero())
                {
                    throw EFutureTimeoutWaitingForResult();
                }
                else if (timeout.IsPositive())
                {
                    MonotonicClock mtc;
                    RealtimeClock rtc;
                    Timespec mtTimeout = mtc.GetTime() + timeout;
                    while (!mResultReady && (mtc.GetTime() <= mtTimeout))
                    {
                        Timespec rtTimeout = rtc.GetTime()
                                + (mtTimeout - mtc.GetTime());
                        mCondition.TimedWait(rtTimeout);
                    }
                    if (!mResultReady)
                        throw EFutureTimeoutWaitingForResult();
                }
                else
                {
                    mCondition.Wait();
                }
            }

            if (mException)
            {
                try
                {
                    boost::rethrow_exception(mException);
                }
                catch (boost::unknown_exception &e)
                {
                    throw EFutureExceptionUnknown();
                }
            }
        }

        virtual void GetResult() {
            GetResultTimed(Timespec::FromSeconds(-1));
        }
        
        virtual void SetResult(void) {
            AutoUnlockMutex lock(mLock);
            if (mResultReady)
                throw EFutureResultAlreadySet();
        }
        virtual void SetException(boost::exception_ptr e) {
            FTRACE;
            AutoUnlockMutex lock(mLock);
            mException = e;
            mResultReady = true;
            mCondition.Broadcast();
        }
    private:
        mutable Forte::Mutex mLock;
        Forte::ThreadCondition mCondition;
        bool mCancelled;
        bool mResultReady;
        boost::exception_ptr mException;
    };
};

#endif
