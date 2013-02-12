// #SCQAD TAG: forte.pdupeer
#ifndef __Forte_PDUPeerFileDescriptorEndpoint_h_
#define __Forte_PDUPeerFileDescriptorEndpoint_h_

#include "AutoMutex.h"
#include "AutoFD.h"
#include "PDUPeer.h"
#include <boost/shared_array.hpp>
#include "Semaphore.h"

namespace Forte
{
    static const int RECV_BUFFER_SIZE = 65536;
    static const int DEFAULT_MAX_BUFFER_SIZE = 1048576;

    class PDUPeerFileDescriptorEndpoint : public PDUPeerEndpoint
    {
    public:
        PDUPeerFileDescriptorEndpoint(
            int fd,
            unsigned int bufSize = RECV_BUFFER_SIZE,
            unsigned int bufMaxSize = DEFAULT_MAX_BUFFER_SIZE,
            unsigned int bufStepSize = RECV_BUFFER_SIZE) :
            mFDLock(),
            mFD(fd),
            mBufferFullCondition(mReceiveMutex),
            mEPollFD(-1),
            mCursor(0),
            mBufSize(bufSize),
            mBufMaxSize(bufMaxSize),
            mBufStepSize(bufStepSize),
            mPDUBuffer(new char[mBufSize])
            {
                FTRACE2("%d", (int) mFD);

                if (mBufMaxSize < mBufSize)
                    mBufMaxSize = mBufSize;
                if (mBufStepSize > mBufSize)
                    mBufStepSize = mBufSize;

                memset(mPDUBuffer.get(), 0, mBufSize);
            }

        virtual ~PDUPeerFileDescriptorEndpoint() {}

        virtual void Begin() {
            //No customization on FD PDUPeer Endpoint
        }

        void SetFD(int FD);
        int GetFD(void) const { return mFD; }
        void DataIn(const size_t len, const char *buf);
        virtual void SendPDU(const Forte::PDU &pdu);

        void SetEPollFD(int epollFD);

        /**
         * Called when you get an epoll event. will call to protected
         * functions below for specific handling
         */
        virtual void HandleEPollEvent(const struct epoll_event& e);

        virtual void TeardownEPoll();

        /**
         * Determine whether a full PDU has been received from the
         * peer, and is ready for the application to get with a
         * RecvPDU call.
         *
         * @return true if a PDU is ready, false otherwise
         */
        bool IsPDUReady(void) const;

        /**
         * Receive a PDU.  Returns true if a PDU was received, false
         * if no complete PDU is ready.
         *
         * @param out
         *
         * @return true if a PDU was received, false if not.
         */
        bool RecvPDU(Forte::PDU &out);

        void Close(void) { mFD.Close(); }

        virtual bool IsConnected(void) const {
            AutoUnlockMutex fdlock(mFDLock);
            return (mFD != -1);
        }

        virtual bool OwnsFD(int fd) const {
            AutoUnlockMutex fdlock(mFDLock);
            return (mFD != -1 && mFD == fd);
        }

    protected:
        virtual void handleFileDescriptorClose(const struct epoll_event& e);
        bool lockedIsPDUReady(void) const;
        void lockedRemoveFDFromEPoll();
        void lockedAddFDToEPoll();

        void lockedDataIn(const size_t len, const char *buf);
        void callbackIfPDUReady();

    protected:
        mutable Forte::Mutex mReceiveMutex;
        mutable Forte::Mutex mSendMutex;

        mutable Forte::Mutex mFDLock;
        AutoFD mFD;
        mutable Forte::ThreadCondition mBufferFullCondition;
        mutable Forte::Mutex mEPollLock;
        int mEPollFD;
        size_t volatile mCursor;
        size_t mBufSize;
        size_t mBufMaxSize;
        size_t mBufStepSize;
        boost::shared_array<char> mPDUBuffer;
    };
};
#endif
