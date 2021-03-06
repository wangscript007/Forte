// #SCQAD TAG: forte.pdupeer
#ifndef __Forte_PDUPeer_h_
#define __Forte_PDUPeer_h_

/**
 * PDUPeer represents a two sided connection, of which the local
 * PDUPeer is one side of a conversation. EnqueuePDU is provided as a
 * way to enqueue a PDU to be delivered to the other side. We will
 * attempt to send PDUs until a deadline for delivery expires. In the
 * event it becomes hopeless to get the PDU delivered, PDUPeer will
 * call an error callback for each PDU it has enqueued.
 *
 * PDUPeer::Enqueue is asynchronous, and will pass the pdu off to it's
 * endpoint while alerting the dispatcher there is work to be done on
 * the Endpoint
 *
 */

#include "Exception.h"
#include "Object.h"
#include "Dispatcher.h"
#include "PDUPeerEndpoint.h"
#include "PDU.h"
#include "FTrace.h"
#include "EnableStats.h"
#include <boost/bind.hpp>
#include "ThreadedObject.h"

EXCEPTION_CLASS(EPDUPeer);

namespace Forte
{
    class Mutex;

    class PDUPeer : public ThreadedObject,
                    public virtual BaseEnableStats
    {
    public:
        PDUPeer() {}
        virtual ~PDUPeer() {}

        // Start is called after the object is fully constructed and
        // allows for smoother interaction between this object and
        // it's thread
        virtual void Start() = 0;

        // Shutdown must be called in pair with Start. It will stop
        // threads and tear down objects that might call into it
        // before the destructor fires
        virtual void Shutdown() = 0;

        // In cases where there is a single FD we will provide it to
        // consumers. This is largely to support existing PDUPeer
        // implementations that rely on the underying file
        // descriptor. will look at changing this in the future
        virtual int GetFD() const = 0;
        virtual bool OwnsFD(int fd) const = 0;
        virtual void AddFD(int fd) = 0;

        // we will add ourselves to the epoll fd that is passed in
        virtual void HandleEPollEvent(const struct epoll_event& e) = 0;

        virtual const uint64_t GetID() const = 0;

        virtual void EnqueuePDU(const boost::shared_ptr<Forte::PDU>& pdu) = 0;

        virtual unsigned int GetQueueSize() const = 0;

        //TOOD:
        //virtual void SendPDU(const PDU& pdu) = 0;

        /**
         * Check with endpoint to see if we are actively connected to
         * our peer.
         *
         * @return true if a endpoint is connected
         */
        virtual bool IsConnected() const = 0;

        /**
         * Determine whether a full PDU has been received from the
         * peer, and is ready for the application to get with a
         * RecvPDU call.
         *
         * @return true if a PDU is ready, false otherwise
         */
        virtual bool IsPDUReady(void) const = 0;

        /**
         * Pull PDU from queue
         *
         * @return true if a PDU could
         */
        virtual bool RecvPDU(Forte::PDU &out) = 0;

        void SetEventCallback(PDUPeerEventCallback f) {
            Forte::AutoUnlockMutex lock(mEventCallbackMutex);
            mEventCallback = f;
        }

        // incoming events from our PDUPeerEndpoint
        virtual void PDUPeerEndpointEventCallback(PDUPeerEventPtr& event) {
            FTRACE;
            PDUPeerEventCallback eventCallback;
            {
                Forte::AutoUnlockMutex lock(mEventCallbackMutex);
                if (mEventCallback)
                {
                    event->mPeer = GetPtr();
                    eventCallback = mEventCallback;
                }
            }
            if (eventCallback)
            {
                eventCallback(event);
            }
        }

        /**
         * Get a shared pointer to this PDUPeer.  NOTE: A shared_ptr
         * to this object must already exist.
         *
         * @return shared_ptr
         */
        PDUPeerPtr GetPtr(void) {
            return boost::static_pointer_cast<PDUPeer>(
                Object::shared_from_this());
        }

    private:
        Forte::Mutex mEventCallbackMutex;
        PDUPeerEventCallback mEventCallback;
    };
};
#endif
