#include "SocketUtil.h"
#include "AutoFD.h"
#include "FTrace.h"
#include "AutoDoUndo.h"
#include <netinet/tcp.h>
#include <fcntl.h>
#include <boost/bind.hpp>
//TODO: this is lifted from linux/tcp.h. if i include linux/tcp.h, i
//#include <linux/tcp.h>
//get these errors:
///usr/include/linux/tcp.h:24: error: redefinition of ‘struct tcphdr’
///usr/include/netinet/tcp.h:93: error: previous definition of ‘struct tcphdr’
///usr/include/linux/tcp.h:72: error: ‘__u32 __fswab32(__u32)’ cannot appear in a constant-expression
#define TCP_USER_TIMEOUT 18 /* How long for loss retry before timeout */

#include "LogManager.h"
#include "SystemCallUtil.h"

#pragma GCC diagnostic ignored "-Wold-style-cast"

int Forte::createSocket(int domain, int type, int protocol)
{
    int sock(0);
    if ((sock = socket(PF_INET, SOCK_STREAM, 0)) == -1)
    {
        hlogstream(HLOG_DEBUG2,
                   "could not create socket. " << errno
                   << ":" << SystemCallUtil::GetErrorDescription(errno)
            );
        throw ECouldNotCreateSocket();
    }
    return sock;
}

int Forte::createInetStreamSocket()
{
    return createSocket(PF_INET, SOCK_STREAM, 0);
}

void Forte::bindToAddress(int fd, const Forte::SocketAddress& bindAddress)
{
    // bind socket and listen
    hlog(HLOG_INFO, "Binding to port %u on %s", bindAddress.second,
         (bindAddress.first.empty()
          ? "all addresses" : bindAddress.first.c_str()));

    struct sockaddr_in bind_addr;
    memset(&bind_addr, 0, sizeof(struct sockaddr_in));
    bind_addr.sin_family = AF_INET;
    bind_addr.sin_port = htons(bindAddress.second);
    if (!inet_aton(bindAddress.first.c_str(), &(bind_addr.sin_addr)))
    {
        throw ECouldNotBind(
            FStringFC(), "invalid bind IP: %s", bindAddress.first.c_str());
    }

    if (::bind(fd,
               reinterpret_cast<struct sockaddr*>(&bind_addr),
               sizeof(struct sockaddr_in)) == -1)
    {
        throw ECouldNotBind(
            FStringFC(),
            "failed to bind: %s",
            SystemCallUtil::GetErrorDescription(errno).c_str());
    }
}

void Forte::connectToAddress(int fd, const Forte::SocketAddress& connectToAddress)
{
    FTRACE2("%s:%d",
            connectToAddress.first.c_str(),
            connectToAddress.second);

    hlogstream(HLOG_DEBUG2, "Attempting connection to "
               << connectToAddress.first << ":" << connectToAddress.second);
    struct sockaddr_in sa;

    if (inet_pton(
            AF_INET, connectToAddress.first.c_str(), &sa.sin_addr) == -1)
    {
        hlogstream(HLOG_DEBUG2,
                   "could not convert ip " << connectToAddress.first
                   << ":" << SystemCallUtil::GetErrorDescription(errno)
            );
        throw ECouldNotConvertIP();
    }

    sa.sin_family = AF_INET;
    sa.sin_port = htons(connectToAddress.second);

    if (::connect(fd,
                  const_cast<const struct sockaddr *>(
                      reinterpret_cast<struct sockaddr*>(&sa)),
                  sizeof(sa)) == -1)
    {
        hlogstream(HLOG_DEBUG2, "could not connect to "
                   << connectToAddress.first
                   << ":" << connectToAddress.second
                   << ":" << SystemCallUtil::GetErrorDescription(errno)
            );

        throw ECouldNotConnect();
    }

    hlogstream(HLOG_INFO,
               "connected to " << connectToAddress.first.c_str()
               << ":" << connectToAddress.second);
}

void Forte::setTCPKeepAlive(int fd,
                            const int soKeepAliveEnabled,
                            const int tcpKeepAliveCount,
                            const int tcpKeepAliveIntervalSeconds,
                            const int tcpKeepAliveIdleSeconds)
{

    if (setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE,
                   &soKeepAliveEnabled,
                   sizeof(soKeepAliveEnabled)) < 0)
    {
        hlog(HLOG_WARN, "Unable to turn on SO Keep Alive for socket %d, %d:%s",
             fd, errno, SystemCallUtil::GetErrorDescription(errno).c_str());
    }
    else
    {
        hlog(HLOG_DEBUG2, "Set SO TCP Keep alive for socket to (%d)",
             soKeepAliveEnabled);

        if (setsockopt(
                fd, SOL_TCP, TCP_KEEPCNT,
                &tcpKeepAliveCount,
                sizeof(tcpKeepAliveCount)) < 0)
        {
            hlog(HLOG_WARN,
                 "Unable to set on TCP_KEEPCNT for socket %d, %d:%s",
                 fd, errno,
                 SystemCallUtil::GetErrorDescription(errno).c_str());
        }
        else
        {
            hlog(HLOG_DEBUG2, "Set TCP_KEEPCNT for socket to (%d)",
                 tcpKeepAliveCount);
        }

        if (setsockopt(
                fd, SOL_TCP, TCP_KEEPINTVL,
                &tcpKeepAliveIntervalSeconds,
                sizeof(tcpKeepAliveIntervalSeconds)) < 0)
        {
            hlog(HLOG_WARN,
                 "Unable to turn on TCP_KEEPINTVL for socket %d, %d:%s",
                 fd, errno,
                 SystemCallUtil::GetErrorDescription(errno).c_str());
        }
        else
        {
            hlog(HLOG_DEBUG2, "Set TCP_KEEPINTVL for socket to (%d)",
                 tcpKeepAliveIntervalSeconds);
        }

        if (setsockopt(
                fd, SOL_TCP, TCP_KEEPIDLE,
                &tcpKeepAliveIdleSeconds,
                sizeof(tcpKeepAliveIdleSeconds)) < 0)
        {
            hlog(HLOG_WARN,
                 "Unable to turn on TCP_KEEPIDLE for socket %d, %d:%s",
                 fd, errno,
                 SystemCallUtil::GetErrorDescription(errno).c_str());
        }
        else
        {
            hlog(HLOG_DEBUG2, "Set TCP_KEEPIDLE for socket to (%d)",
                 tcpKeepAliveIdleSeconds);
        }

    }
}

void Forte::setTCPUserTimeout(int fd, const int tcpUserTimeoutInMillisec)
{
    if (setsockopt(
            fd, SOL_TCP, TCP_USER_TIMEOUT,
            &tcpUserTimeoutInMillisec,
            sizeof(tcpUserTimeoutInMillisec)) < 0)
    {
        hlog(HLOG_WARN,
             "Unable to set on TCP_USER_TIMEOUT for socket, %s",
             SystemCallUtil::GetErrorDescription(errno).c_str());
    }
    else
    {
        hlog(HLOG_DEBUG2, "Set TCP_USER_TIMEOUT for socket to (%d)",
             tcpUserTimeoutInMillisec);
    }
}

void Forte::setReuseAddr(int fd, const int soResuseAddr)
{
    // set SO_REUSEADDR
    if (setsockopt(
            fd,
            SOL_SOCKET,
            SO_REUSEADDR,
            &soResuseAddr,
            sizeof(soResuseAddr)) == -1)
    {
        hlog(HLOG_WARN,
             "Unable to set on SO_REUSEADDR for socket, %s",
             SystemCallUtil::GetErrorDescription(errno).c_str());
    }
    else
    {
        hlog(HLOG_DEBUG2,
             "Set SO_REUSEADDR for socket to (%d)",
             soResuseAddr);
    }
}

void Forte::setSocketNonBlocking(int fd)
{
    int flags, s;
    flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1)
    {
        throw EFcntlFailed(SystemCallUtil::GetErrorDescription(errno));
    }

    flags |= O_NONBLOCK;
    s = fcntl(fd, F_SETFL, flags);
    if (s == -1)
    {
        throw EFcntlFailed(SystemCallUtil::GetErrorDescription(errno));
    }
}

void Forte::setSocketNonBlockingSaveOldFlags(int fd, int &oldFlags)
{
    int flags, s;
    flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1)
    {
        throw EFcntlFailed(SystemCallUtil::GetErrorDescription(errno));
    }
    oldFlags = flags;

    flags |= O_NONBLOCK;
    s = fcntl(fd, F_SETFL, flags);
    if (s == -1)
    {
        throw EFcntlFailed(SystemCallUtil::GetErrorDescription(errno));
    }
}

void Forte::restoreSocketFlags(int fd, int flags)
{
    if (fcntl(fd, F_SETFL, flags) < 0)
    {
        throw EFcntlFailed(SystemCallUtil::GetErrorDescription(errno));
    }
}

void Forte::connectNonBlocking(
    int fd,
    const struct sockaddr *addr,
    socklen_t addrlen,
    const Forte::Timespec &timeout)
{
    int error = 0, ret = 0;
    fd_set rset, wset;
    socklen_t len = sizeof(error);
    struct timeval  ts;

    ts.tv_sec = timeout.AsSeconds();
    ts.tv_usec = (timeout.GetNanosecs() / 1000);

    FD_ZERO(&rset);
    FD_SET(fd, &rset);
    wset = rset;

    int oldSocketFlags;
    setSocketNonBlockingSaveOldFlags(fd, oldSocketFlags);
    Forte::AutoDoUndo autoRestoreFlags(
        boost::bind(&Forte::restoreSocketFlags,
                    fd, oldSocketFlags));

    //initiate non-blocking connect
    if((ret = connect(fd, addr, addrlen)) < 0)
    {
        if (errno != EINPROGRESS)
        {
            throw ECouldNotConnect(FStringFC(), "Connect on socket is not in "
                                 "progress after connect call - %i - %s",
                                 errno,
                                 SystemCallUtil::GetErrorDescription(errno).c_str());
        }
    }

    if(ret == 0) //then connect succeeded right away
    {
        return;
    }

    //we are waiting for connect to complete now
    if((ret = select(fd + 1, &rset, &wset, NULL,
                     (!timeout.IsZero()) ? &ts : NULL)) < 0)
    {
        throw ECouldNotConnect("select call failed during connect");
    }

    if(ret == 0)
    {
        throw ECouldNotConnect("Timed out during connect call");
    }

    //we had a positive return so a descriptor is ready
    if (FD_ISSET(fd, &rset) || FD_ISSET(fd, &wset))
    {
        if(getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len) < 0)
        {
            throw ECouldNotConnect("Error during getsockopt call");
        }

        if(error)
        {  //check if we had a socket error
            throw ECouldNotConnect("Got a socket error");
        }
    }
    else
    {
        throw ECouldNotConnect("Timed out during connect call");
    }
}

int Forte::getTCPSendBufferSize(int fd)
{
    int tcpBufferSize = 0;
    socklen_t tcpBufferSizeLength = sizeof(tcpBufferSize);
    if (getsockopt(fd,
                   SOL_SOCKET,
                   SO_SNDBUF,
                   &tcpBufferSize,
                   &tcpBufferSizeLength) == -1)
    {
        hlog_and_throw(HLOG_WARN, ESocketUtil(
                           FStringFC(), "could not read tcp buffer size: %d",
                           errno));
    }

    return tcpBufferSize;
}

void Forte::setTCPSendBufferSize(int fd, int tcpBufferSize)
{
    socklen_t tcpBufferSizeLength = sizeof(tcpBufferSize);
    if (setsockopt(fd,
                   SOL_SOCKET,
                   SO_SNDBUF,
                   &tcpBufferSize,
                   tcpBufferSizeLength) == -1)
    {
        hlog_and_throw(HLOG_WARN, ESocketUtil(
                           FStringFC(), "could not set tcp buffer size: %d",
                           errno));
    }
}

void Forte::setTCPNoDelay(int fd)
{
    int tcpNoDelay = 1;
    if (setsockopt(fd,
                   IPPROTO_TCP,
                   TCP_NODELAY,
                   &tcpNoDelay,
                   sizeof (int)) == -1)
    {
        hlog_and_throw(HLOG_WARN, ESocketUtil(
                           FStringFC(), "could not set TCP_NODELAY: %d",
                           errno));
    }
}
void Forte::setTCPQuickAck(int fd)
{
    int tcpQuickAck = 1;
    if (setsockopt(fd,
                   IPPROTO_TCP,
                   TCP_QUICKACK,
                   &tcpQuickAck,
                   sizeof (int)) == -1)
    {
        hlog_and_throw(HLOG_WARN, ESocketUtil(
                           FStringFC(), "could not set TCP_QUICKACK: %d",
                           errno));
    }
}
void Forte::waitForSocketAvailable(int fd, int type, Timespec timeout)
{
    fd_set rset, wset;
    fd_set *prset = &rset, *pwset = &wset;
    int res = 0;

    FD_ZERO(&rset);
    FD_ZERO(&wset);

    switch (type)
    {
    case SocketUtil::SOCKET_RDONLY:
            FD_SET(fd, &rset);
            pwset = NULL;
            break;
    case SocketUtil::SOCKET_WRONLY:
            prset = NULL;
            FD_SET(fd, &wset);
            break;
    case SocketUtil::SOCKET_RDWR:
            FD_SET(fd, &rset);
            FD_SET(fd, &wset);
            break;
    default:
        throw(ESocketUtil(FString(
                              FStringFC(), "Unknown socket type: %d", type)));
    }

    struct timeval tv;
    tv.tv_sec = timeout.AsSeconds();
    tv.tv_usec = timeout.GetNanosecs() / 1000;
    res = select(fd + 1, prset, pwset, NULL, &tv);
    if (res <= 0)
    {
        if (res == 0)
            throw ESelectFailed("timed out");
        else
            throw ESelectFailed(SystemCallUtil::GetErrorDescription(errno));
    }
    FD_CLR(fd, &rset);
    FD_CLR(fd, &wset);
}
