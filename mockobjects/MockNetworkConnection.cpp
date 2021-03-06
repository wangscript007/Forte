#include "DaemonUtil.h"
#include "MockNetworkConnection.h"
#include <sys/wait.h>

using namespace Forte;

class MockThread : public Thread
{
public:
    MockThread(const MockNetworkConnection::MockNetworkNodePtr& node,
               int fd,
               bool exit) :
        mHasException (false),
        mNode (node),
        mFD (fd),
        mExit (exit)
        {initialized();};
    virtual ~MockThread() {deleting(); };

    bool mHasException;

protected:
    void *run (void) {

        try
        {
            mNode->runAndCloseFD(mFD, mExit);
        }
        catch(std::exception &e)
        {
            hlog(HLOG_WARN, "Got exception in runAndCloseFD: %s", e.what());
            mHasException = true;
        }

        return NULL;
    };

    MockNetworkConnection::MockNetworkNodePtr mNode;
    int mFD;
    bool mExit;

};

MockNetworkConnection::MockNetworkConnection(
    const MockNetworkConnection::MockNetworkNodePtr& nodeWhoseStateIsSaved,
    const MockNetworkConnection::MockNetworkNodePtr& nodeWhoseStateIsLost) :
    mNodeWhoseStateIsSaved (nodeWhoseStateIsSaved),
    mNodeWhoseStateIsLost (nodeWhoseStateIsLost)
{
}

MockNetworkConnection::~MockNetworkConnection()
{
}

void MockNetworkConnection::RunMockNetwork()
{
    int fds[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds) < 0)
        throw EUnableToCreateSocketPair();
    int parentfd = fds[0];
    int childfd = fds[1];

    pid_t childPid = DaemonUtil::ForkSafely();
    if (childPid < 0)
    {
        close(parentfd);
        close(childfd);
        throw EUnableToForkNetworkNode();
    }
    else if (childPid == 0)
    {

        // child
        // NOTE: the network node whose state is not to
        // be saved is run on the child process.

        // first close the parent fd
        close(parentfd);

        // create the MockThread
        // now run the network node
        MockThread thread(mNodeWhoseStateIsLost, childfd, true);
        thread.WaitForShutdown();
    }
    else
    {
        // parent
        // MockThread thread;

        // first close the child fd
        close(childfd);

        // now run the network node
        MockThread thread(mNodeWhoseStateIsSaved, parentfd, false);

        thread.WaitForShutdown();

        if (thread.mHasException)
        {
            hlog(HLOG_DEBUG,
                 "Got exception in thread.  Sending %d SIGINT...", childPid);

            // wait for all child pids to finish and then throw
            kill(childPid, SIGTERM);
            hlog(HLOG_DEBUG, "Child pid %d killed. Waiting for it to finish",
                 childPid);
            int status;
            ::wait(&status);
            hlog(HLOG_DEBUG, "Done waiting for child pid %d", childPid);


            throw ENodeHasException();
        }
        hlog(HLOG_DEBUG, "Waiting for all children to finish");
        // wait for all child pids to finish
        int status;
        ::wait(&status);
        hlog(HLOG_DEBUG, "All children have finished");
    }
}
