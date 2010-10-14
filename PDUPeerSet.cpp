#include <sys/epoll.h>
#include "PDUPeerSet.h"
#include "Foreach.h"
#include "LogManager.h"
#include "Types.h"

// \TODO remove this once EPOLLRDHUP makes it into sys/epoll.h
#ifndef EPOLLRDHUP
#define EPOLLRDHUP 0x2000
#endif

using namespace boost;
using namespace Forte;

void Forte::PDUPeerSet::PeerDelete(const shared_ptr<PDUPeer> &peer)
{
    AutoUnlockMutex lock(mLock);
    peerDeleteLocked(peer);
}

void Forte::PDUPeerSet::peerDeleteLocked(const shared_ptr<PDUPeer> &peer)
{
    if (peer)
    {
        if (mEPollFD != -1 &&
            epoll_ctl(mEPollFD, EPOLL_CTL_DEL, peer->GetFD(), NULL) < 0)
            hlog(HLOG_ERR, "EPOLL_CTL_DEL failed: %s", strerror(errno));
        // erase from FD map
        mPeerSet.erase(peer);
    }
}
shared_ptr<PDUPeer> Forte::PDUPeerSet::PeerCreate(int fd)
{
    AutoUnlockMutex lock(mLock);
    shared_ptr<PDUPeer> peer(new PDUPeer(fd));
    mPeerSet.insert(peer);
    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLERR | EPOLLRDHUP;
    ev.data.ptr = peer.get();
    if (mEPollFD != -1 &&
        epoll_ctl(mEPollFD, EPOLL_CTL_ADD, fd, &ev) < 0)
    {
        mPeerSet.erase(peer);
        throw EPDUPeerSetPollAdd(strerror(errno));
    }
    return peer;
}

void Forte::PDUPeerSet::SendAll(const PDU &pdu) const
{
    AutoUnlockMutex lock(mLock);
    foreach (const shared_ptr<PDUPeer> &peer, mPeerSet)
    {
        if (!peer) continue;
        hlog(HLOG_DEBUG, "sending to peer on fd %d", 
             peer->GetFD());
        try
        {
            peer->SendPDU(pdu);
        }
        catch (EPeerSendFailed &e)
        {
            hlog(HLOG_WARN, "%s", e.what().c_str());
        }
    }
}

int Forte::PDUPeerSet::SetupEPoll(void)
{
    AutoUnlockMutex lock(mLock);
    if (mEPollFD != -1)
        return mEPollFD; // already set up
    AutoFD fd = epoll_create(4);
    if (fd == -1)
        throw EPDUPeerSetPollCreate(strerror(errno));
    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLERR | EPOLLRDHUP;

    foreach (const shared_ptr<PDUPeer> &p, mPeerSet)
    {
        ev.data.ptr = p.get();
        if (epoll_ctl(fd, EPOLL_CTL_ADD, p->GetFD(), &ev) < 0)
            throw EPDUPeerSetPollAdd(strerror(errno));
    }
    mBuffer.reset(new char[RECV_BUFFER_SIZE]);
    mEPollFD = fd.Release();
    hlog(HLOG_DEBUG, "created epoll descriptor on fd %d", mEPollFD);
    return mEPollFD;
}

void Forte::PDUPeerSet::TeardownEPoll(void)
{
    AutoUnlockMutex lock(mLock);
    if (mEPollFD != -1)
    {
        close(mEPollFD);
        mEPollFD = -1;
        mBuffer.reset();
    }
}

void Forte::PDUPeerSet::Poll(int msTimeout)
{
    AutoUnlockMutex lock(mLock);
    if (mEPollFD == -1 || !mBuffer)
        throw EPDUPeerSetNotPolling();
    struct epoll_event events[32];
    int nfds = epoll_wait(mEPollFD, events, 32, msTimeout);
//    hlog(HLOG_DEBUG, "epoll_wait complete, nfds=%d", nfds);
    if (nfds < 0 &&
        ((errno == EBADF ||
          errno == EFAULT ||
          errno == EINVAL) &&
         errno != EINTR))
    {
        throw EPDUPeerSetPollFailed(strerror(errno));
    }
    // process the fds
    char *buffer = mBuffer.get();
    for (int i = 0; i < nfds; ++i)
    {
//        hlog(HLOG_DEBUG, "i=%d events=0x%x", i, events[i].events);
        bool error = false;
        PDUPeer *p = reinterpret_cast<PDUPeer *>(events[i].data.ptr);
        const shared_ptr<PDUPeer> &peer(p->GetPtr());
        if (!peer)
        {
            hlog(HLOG_CRIT, "invalid peer in epoll data");
            throw EPDUPeerSetPollFailed();
        }
        if (events[i].events & EPOLLIN)
        {
            // input is ready
            int len = recv(peer->GetFD(), buffer, RECV_BUFFER_SIZE, 0);
            if (len < 0)
            {
                hlog(HLOG_ERR, "recv failed: %s", strerror(errno));
                error = true;
            }
            else if (len == 0)
            {
                hlog(HLOG_DEBUG, "socket shutdown");
                error = true;
            }
            else
            {
                hlog(HLOG_DEBUG, "received %llu bytes", (u64) len);
                peer->DataIn(len, buffer);
                if (mProcessPDUCallback && peer->IsPDUReady())
                    mProcessPDUCallback(*peer);
            }
        }
        if (error ||
            events[i].events & EPOLLERR ||
            events[i].events & EPOLLHUP ||
            events[i].events & EPOLLRDHUP)
        {
            // disconnected, or needs a disconnect
            hlog(HLOG_DEBUG, "deleting peer on fd %d", peer->GetFD());
            peerDeleteLocked(peer);
            if (mErrorCallback)
                mErrorCallback(*peer);
            peer->Close();
        }
    }
}
