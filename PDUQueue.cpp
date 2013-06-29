// #SCQAD TAG: forte.pdupeer
#include "PDUQueue.h"
#include "SystemCallUtil.h"
#include "LogManager.h"
#include "Types.h"
#include "PDUPeerTypes.h"
#include "Thread.h"
#include <boost/make_shared.hpp>

using namespace boost;
using namespace Forte;

PDUQueue::PDUQueue(
    long pduSendTimeout,
    unsigned short queueSize,
    PDUPeerQueueType queueType)
    : mPDUSendTimeout(pduSendTimeout),
      mPDUQueueMutex(),
      mPDUQueueNotEmptyCondition(mPDUQueueMutex),
      mQueueMaxSize(queueSize),
      mQueueType(queueType),
      mQueueSemaphore(mQueueMaxSize),
      mTotalQueued(0),
      mQueueSize(0),
      mAvgQueueSize()
{
    FTRACE;
    registerStatVariable<0>("totalQueued", &PDUQueue::mTotalQueued);
    registerStatVariable<1>("queueSize", &PDUQueue::mQueueSize);
    registerStatVariable<2>("averageQueueSize", &PDUQueue::mAvgQueueSize);
}

PDUQueue::~PDUQueue()
{
    FTRACE;
}

void PDUQueue::EnqueuePDU(const PDUPtr& pdu)
{
    FTRACE;

    if (mQueueType == PDU_PEER_QUEUE_BLOCK)
    {
        mQueueSemaphore.Wait();
    }

    // Potential race condition between semaphore & mutex locks
    AutoUnlockMutex lock(mPDUQueueMutex);

    mTotalQueued++;
    mQueueSize = mPDUQueue.size();
    mAvgQueueSize = mPDUQueue.size();

    if (mQueueType != PDU_PEER_QUEUE_BLOCK && mPDUQueue.size()+1 > mQueueMaxSize)
    {
        switch (mQueueType)
        {
        case PDU_PEER_QUEUE_CALLBACK:
        case PDU_PEER_QUEUE_THROW:
            hlog_and_throw(HLOG_ERR, EPDUQueueFull(mQueueMaxSize));
            return;
        default:
            hlog_and_throw(HLOG_ERR,
                           EPDUQueueUnknownType(mQueueType));
            return;
        }
    }

    PDUHolderPtr pduHolder(new PDUHolder);
    pduHolder->enqueuedTime = mClock.GetTime();
    pduHolder->pdu = pdu;
    mPDUQueue.push_back(pduHolder);

    mPDUQueueNotEmptyCondition.Signal();
}

void PDUQueue::GetNextPDU(boost::shared_ptr<PDU>& pdu)
{
    AutoUnlockMutex lock(mPDUQueueMutex);
    if (mPDUQueue.size() > 0)
    {
        pdu = mPDUQueue.front()->pdu;
        mPDUQueue.pop_front();
        mQueueSize = mPDUQueue.size();
        mAvgQueueSize = mPDUQueue.size();
        mQueueSemaphore.Post();
    }
}

void PDUQueue::WaitForNextPDU(PDUPtr& pdu)
{
    AutoUnlockMutex lock(mPDUQueueMutex);
    while (!Thread::MyThread()->IsShuttingDown()
           && mPDUQueue.size() == 0)
    {
        mPDUQueueNotEmptyCondition.Wait();
    }

    if (mPDUQueue.size() > 0)
    {
        pdu = mPDUQueue.front()->pdu;
        mPDUQueue.pop_front();
        mQueueSize = mPDUQueue.size();
        mAvgQueueSize = mPDUQueue.size();
        mQueueSemaphore.Post();
    }
    else if (Thread::MyThread()->IsShuttingDown())
    {
        mQueueSemaphore.Post();
    }
}

bool PDUQueue::isPDUExpired(PDUHolderPtr pduHolder)
{
    //FTRACE;

    Timespec timeout(mPDUSendTimeout, 0);
    Timespec now;
    now = mClock.GetTime();
    if (pduHolder->enqueuedTime + timeout < now)
    {
        return true;
    }

    return false;
}

void PDUQueue::failExpiredPDUs()
{
    FTRACE;

    //std::vector<PDUPeerEventPtr> events;

    {
        AutoUnlockMutex lock(mPDUQueueMutex);
        PDUHolderPtr pduHolder;
        // newest items will be at the back. loop until we find one that
        // is not expired or the list is emtpy
        while (!mPDUQueue.empty() && isPDUExpired(mPDUQueue.front()))
        {
            pduHolder = mPDUQueue.front();
            if (isPDUExpired(pduHolder))
            {
                //TODO: nothing uses this right now
                /*if (mEventCallback)
                {
                    PDUPeerEventPtr event(new PDUPeerEvent());
                    event->mPeer = GetPtr();
                    event->mEventType = PDUPeerSendErrorEvent;
                    event->mPDU = pduHolder->pdu;
                    events.push_back(event);
                    }*/
                mPDUQueue.pop_front();
                mQueueSemaphore.Post();
            }
        }
    }

    // can't call this with mutex
    //foreach(const PDUPeerEventPtr& event, events)
    //{
    //mEventCallback(event);
    //}
}