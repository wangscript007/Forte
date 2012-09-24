#ifndef __PDU_peer_set_h_
#define __PDU_peer_set_h_

#include "AutoFD.h"
#include "AutoMutex.h"
#include "PDUPeer.h"
#include "RWLock.h"
#include <boost/function.hpp>
#include <boost/shared_array.hpp>

EXCEPTION_CLASS(EPDUPeerSet);

EXCEPTION_SUBCLASS2(EPDUPeerSet,
                    EPDUPeerDuplicate,
                    "Peer already exists");

EXCEPTION_SUBCLASS2(EPDUPeerSet,
                    EPDUPeerInvalid,
                    "Invalid Peer");

EXCEPTION_SUBCLASS2(EPDUPeerSet,
                    EPDUPeerSetPollFailed,
                    "Failed to poll");

EXCEPTION_SUBCLASS2(EPDUPeerSet,
                    EPDUPeerSetPollCreate,
                    "Failed to create poll socket");

EXCEPTION_SUBCLASS2(EPDUPeerSet,
                    EPDUPeerSetPollAdd,
                    "Failed to add descriptor to poll socket");

EXCEPTION_SUBCLASS2(EPDUPeerSet,
                    EPDUPeerSetNotPolling,
                    "PDUPeerSet is not set up for polling");

EXCEPTION_SUBCLASS2(EPDUPeerSet,
                    EPDUPeerSetNoPeers,
                    "No peers have been added for polling");

namespace Forte
{
    /**
     * PDUPeerSet manages a set of PDUPeer objects, and provides a
     * mechanism for polling the entire set for input simultaneously.
     * 'PDU ready' and 'error' callbacks may be used as part of the
     * polling mechanism.
     */
    class PDUPeerSet : public Object
    {
    public:
        typedef boost::function<void(PDUPeer &peer)> CallbackFunc;

        PDUPeerSet() {}
        virtual ~PDUPeerSet() {}

        /**
         * GetSize() returns the current number of PDUPeer objects
         * being managed by this PDUPeerSet.
         *
         * @return size of the set
         */
        virtual unsigned int GetSize(void) = 0;

        /**
         * PeerCreate will create a new PDUPeer object for the already
         * open peer connection on the given file descriptor.
         *
         * @param fd
         *
         * @return
         */
        virtual boost::shared_ptr<PDUPeer> PeerCreate(int fd) = 0;

        /**
         * SendAll will send the given PDU to ALL of the peers being
         * managed by this PDUPeerSet.
         *
         * @param pdu
         */
        virtual void SendAll(const PDU &pdu) const = 0;

        /**
         * SetProcessPDUCallback sets the single callback function to
         * use when a complete PDU has been received on any of the
         * peer connections.
         *
         * @param f
         */
        virtual void SetProcessPDUCallback(CallbackFunc f) = 0;

        /**
         * SetErrorCallback sets the single callback function to use
         * when an unrecoverable error has occurred on any of the peer
         * connections.  The file descriptor within the errored
         * PDUPeer will still be valid at the time the callback is
         * made, but will be closed immediately after the callback
         * returns.
         *
         * @param f
         */
        virtual void SetErrorCallback(CallbackFunc f) = 0;

        /**
         * Creates an epoll file descriptor, and automatically adds
         * all PDUPeer file descriptors to it for polling.  Once
         * created, all subsequent calls to PeerConnected() and
         * fdDisconnected() will add / remove those FDs from the
         * epoll file descriptor, until TeardownEPoll() is called.
         *
         * @return int epoll file descriptor
         */
        virtual int SetupEPoll(void) = 0;

        /**
         * Closes the epoll file descriptor (removing all existing
         * peer descriptors from polling).  If another caller is
         * currently blocked on Poll(), they will receive an
         * exception.
         *
         */
        virtual void TeardownEPoll(void) = 0;

        /**
         * Poll will poll all current Peers for input, and process any
         * received input via the ProcessPDUCallback.  The callback
         * will be called for each ready PDU until no more fully
         * buffered PDUs exist for each peer in succession.  If the
         * epoll_wait() call is interrupted, Poll() will return
         * immediately.
         *
         * @param msTimeout timeout in milliseconds before returning.
         * A value of -1 (the default) will wait indefinitely, while a
         * value of 0 will not wait at all.
         * @param interruptible will return if the epoll syscall is
         * interrupted by a signal.
         *
         */
        virtual void Poll(int msTimeout = -1, bool interruptible = false) = 0;

        /**
         * PeerDelete will delete the given peer from the PDUPeerSet,
         * and remove the peer from any poll operation in progress.
         *
         * @param peer
         */
        virtual void PeerDelete(
            const boost::shared_ptr<Forte::PDUPeer> &peer) = 0;
    };
    typedef boost::shared_ptr<Forte::PDUPeerSet> PDUPeerSetPtr;
};
#endif
