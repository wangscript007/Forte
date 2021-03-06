// ProcessFutureImpl.cpp

#include "ProcessFutureImpl.h"
#include "ProcessManager.h"
#include "AutoMutex.h"
#include "Clock.h"
#include "Exception.h"
#include "LogManager.h"
#include "LogTimer.h"
#include "ServerMain.h"
#include "Util.h"
#include "FTrace.h"
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <csignal>
#include <ctime>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <boost/algorithm/string.hpp>

using namespace std;
using namespace boost;
using namespace Forte;

Forte::ProcessFutureImpl::ProcessFutureImpl(
    const boost::shared_ptr<ProcessManagerImpl> &mgr,
    const FString &command,
    const FString &currentWorkingDirectory,
    const FString &outputFilename,
    const FString &errorFilename,
    const FString &inputFilename,
    const StrStrMap *environment,
    const FString &commandToLog) :
    mProcessManagerPtr(mgr),
    mCommand(command),
    mCommandToLog(commandToLog),
    mCurrentWorkingDirectory(currentWorkingDirectory),
    mOutputFilename(outputFilename),
    mErrorFilename(errorFilename),
    mInputFilename(inputFilename),
    mMonitorPid(-1),
    mProcessPid(-1),
    mOutputString(""),
    mErrorString(""),
    mState(STATE_READY),
    mWaitCond(mWaitLock)
{
    // copy the environment entries
    if(environment)
    {
        mEnvironment.insert(environment->begin(), environment->end());
    }
}

Forte::ProcessFutureImpl::~ProcessFutureImpl()
{
    try
    {
        abandon();
    }
    catch (EProcessFutureHandleInvalid &e)
    {
        // ProcessManager has been destroyed
    }
    catch (Exception &e)
    {
        hlog(HLOG_ERR, "%s", e.what());
    }
    catch (std::exception &e)
    {
        hlog(HLOG_ERR, "%s", e.what());
    }
    catch (...)
    {
        hlog(HLOG_ERR, "Unknown exception!");
    }
}

void Forte::ProcessFutureImpl::SetProcessCompleteCallback(ProcessCompleteCallback processCompleteCallback)
{
    if(getState() != STATE_READY)
    {
        hlog(HLOG_ERR, "tried setting the process complete callback after the process had been started");
        throw EProcessFutureStarted();
    }
    mProcessCompleteCallback = processCompleteCallback;
}

void Forte::ProcessFutureImpl::SetCurrentWorkingDirectory(const FString &cwd)
{
    if(getState() != STATE_READY)
    {
        hlog(HLOG_ERR, "tried setting the current working directory after the process had been started");
        throw EProcessFutureStarted();
    }
    mCurrentWorkingDirectory = cwd;
}

void Forte::ProcessFutureImpl::SetEnvironment(const StrStrMap *env)
{
    int state = getState();
    if(state != STATE_READY)
    {
        hlog(HLOG_ERR, "tried setting the environment after the process had been started");
        throw EProcessFutureStarted();
    }
    if(env)
    {
        mEnvironment.clear();
        mEnvironment.insert(env->begin(), env->end());
    }
}

void Forte::ProcessFutureImpl::SetInputFilename(const FString &infile)
{
    int state = getState();

    if(state != STATE_READY)
    {
        hlog(HLOG_ERR, "tried setting the input filename after the process had been started");
        throw EProcessFutureStarted();
    }
    mInputFilename = infile;
}

void Forte::ProcessFutureImpl::SetOutputFilename(const FString &outfile)
{
    int state = getState();

    if(state != STATE_READY)
    {
        hlog(HLOG_ERR, "tried setting the output filename after the process had been started");
        throw EProcessFutureStarted();
    }
    mOutputFilename = outfile;
}

void Forte::ProcessFutureImpl::SetErrorFilename(const FString &errorfile)
{
    int state = getState();

    if(state != STATE_READY)
    {
        hlog(HLOG_ERR, "tried setting the error filename after the process had been started");
        throw EProcessFutureStarted();
    }
    mErrorFilename = errorfile;
}

boost::shared_ptr<ProcessManagerImpl> Forte::ProcessFutureImpl::getProcessManager(void)
{
    boost::shared_ptr<ProcessManagerImpl> mgr(mProcessManagerPtr.lock());
    if (!mgr)
    {
        throw EProcessFutureHandleInvalid();
    }
    return mgr;
}

void Forte::ProcessFutureImpl::run()
{
    if (!mManagementChannel)
        throw EProcessFutureHandleInvalid();

    if (getState() != STATE_READY)
        throw EProcessFutureStarted();

    // we must set the state prior to sending the start PDU to avoid a race
    setState(STATE_STARTING);

    try
    {

        // send the param PDUs, with full command line info, etc
        PDUPtr paramPDU(new PDU(ProcessOpParam, sizeof(ProcessParamPDU)));
        ProcessParamPDU *param = paramPDU->GetPayload<ProcessParamPDU>();

        // \TODO safe copy of these strings, ensure null termination,
        // disallow truncation (throw exception if the source strings are
        // too long)
        strncpy(param->str, mCommand.c_str(), sizeof(param->str));
        param->param = ProcessCmdline;
        mManagementChannel->EnqueuePDU(paramPDU);

        // PDU: command to log
        paramPDU.reset(new PDU(ProcessOpParam, sizeof(ProcessParamPDU)));
        param = paramPDU->GetPayload<ProcessParamPDU>();
        strncpy(param->str, mCommandToLog.c_str(), sizeof(param->str));
        param->param = ProcessCmdlineToLog;
        mManagementChannel->EnqueuePDU(paramPDU);

        paramPDU.reset(new PDU(ProcessOpParam, sizeof(ProcessParamPDU)));
        param = paramPDU->GetPayload<ProcessParamPDU>();
        strncpy(param->str, mCurrentWorkingDirectory.c_str(), sizeof(param->str));
        param->param = ProcessCwd;
        mManagementChannel->EnqueuePDU(paramPDU);

        paramPDU.reset(new PDU(ProcessOpParam, sizeof(ProcessParamPDU)));
        param = paramPDU->GetPayload<ProcessParamPDU>();
        strncpy(param->str, mInputFilename.c_str(), sizeof(param->str));
        param->param = ProcessInfile;
        mManagementChannel->EnqueuePDU(paramPDU);

        paramPDU.reset(new PDU(ProcessOpParam, sizeof(ProcessParamPDU)));
        param = paramPDU->GetPayload<ProcessParamPDU>();
        strncpy(param->str, mOutputFilename.c_str(), sizeof(param->str));
        param->param = ProcessOutfile;
        mManagementChannel->EnqueuePDU(paramPDU);

        paramPDU.reset(new PDU(ProcessOpParam, sizeof(ProcessParamPDU)));
        param = paramPDU->GetPayload<ProcessParamPDU>();
        strncpy(param->str, mErrorFilename.c_str(), sizeof(param->str));
        param->param = ProcessErrfile;
        mManagementChannel->EnqueuePDU(paramPDU);

        // send the control PDU telling the process to start
        PDUPtr pdu(new PDU(ProcessOpControlReq, sizeof(ProcessControlReqPDU)));
        ProcessControlReqPDU *control = pdu->GetPayload<ProcessControlReqPDU>();

        control->control = ProcessControlStart;
        mManagementChannel->EnqueuePDU(pdu);
    }
    catch (Exception &e)
    {
        throw EProcessFutureManagementProcFailed(e.what());
    }

    // wait for the process to change state
    DeadlineClock deadline;
    deadline.ExpiresInSeconds(5);
    MonotonicClock mtc;
    AutoUnlockMutex lock(mWaitLock);
    while (!deadline.Expired() &&
           getState() == STATE_STARTING)
    {
        mWaitCond.TimedWait(mtc.GetTime() + Timespec::FromMillisec(100));
    }

    if (deadline.Expired() &&
        getState() == STATE_STARTING)
    {
        // get's cancel the process monitor
        Cancel();

        hlog_and_throw(
            HLOG_WARN,
            EProcessFutureManagementProcFailed(
                FString(
                    FStringFC(),
                    "Timed out waiting for process monitor to start for command %s",
                    mCommand.c_str())));
    }
}

void Forte::ProcessFutureImpl::setState(int state)
{
    {
        AutoUnlockMutex lock(mStateLock);
        mState = state;
    }
    if (isInTerminalState())
    {
        if (mProcessCompleteCallback)
        {
            mProcessCompleteCallback(static_pointer_cast<ProcessFuture>(shared_from_this()));
        }

        // set the future
        try
        {
            switch (getState())
            {
            case STATE_EXITED:
            {
                // first check the exit code is 0
                if (mStatusCode == 0)
                {
                    // set the future as completed fine
                    SetException(boost::exception_ptr());
                }
                else
                {
                    // throw a boost::throw with non-zero status code
                    boost::throw_exception(
                        EProcessFutureTerminatedWithNonZeroStatus(
                            FStringFC(), "[%s] %u", mCommand.c_str(),
                            mStatusCode)
                        );
                }
                break;
            }

            case STATE_KILLED:
            {
                // throw exception saying the process was killed
                boost::throw_exception(EProcessFutureKilled());
                break;
            }

            case STATE_ERROR:
            {
                switch (mStatusCode)
                {
                case ProcessUnableToOpenInputFile:
                    boost::throw_exception(
                        EProcessFutureUnableToOpenInputFile(mErrorString));
                    break;
                case ProcessUnableToOpenOutputFile:
                    boost::throw_exception(
                        EProcessFutureUnableToOpenOutputFile(mErrorString));
                    break;
                case ProcessUnableToOpenErrorFile:
                    boost::throw_exception(
                        EProcessFutureUnableToOpenErrorFile(mErrorString));
                    break;
                case ProcessUnableToCWD:
                    boost::throw_exception(
                        EProcessFutureUnableToCWD(mErrorString));
                    break;
                case ProcessUnableToFork:
                    boost::throw_exception(
                        EProcessFutureUnableToFork(mErrorString));
                    break;
                case ProcessUnableToExec:
                    boost::throw_exception(
                        EProcessFutureUnableToExec(mErrorString));
                    break;
                case ProcessProcmonFailure:
                    boost::throw_exception(
                        EProcessFutureManagementProcFailed());
                    break;
                default:
                    boost::throw_exception(
                        EProcessFuture(mErrorString));
                    break;
                }
                break;
            }

            case STATE_ABANDONED:
            {
                boost::throw_exception(
                    EProcessFutureAbandoned());
            }

            default:
            {
                // throw exception saying the process ended due to unknown reasons
                boost::throw_exception(
                    EProcessFutureTerminatedDueToUnknownReason());
            }
            }
        }
        catch (...)
        {
            // we got an exception which we need to
            // record in the future
            SetException(boost::current_exception());
        }

    }

    AutoUnlockMutex lock(mWaitLock);
    mWaitCond.Broadcast();
}

void Forte::ProcessFutureImpl::GetResult()
{
    if(getState() == STATE_READY)
    {
        hlog(HLOG_ERR, "tried waiting on a process that has not been started");
        throw EProcessFutureNotRunning();
    }

    Future<void>::GetResult();
}

void Forte::ProcessFutureImpl::GetResultTimed(const Timespec &timeout)
{
    if(getState() == STATE_READY)
    {
        hlog(HLOG_ERR, "tried waiting on a process that has not been started");
        throw EProcessFutureNotRunning();
    }

    Future<void>::GetResultTimed(timeout);
}


void Forte::ProcessFutureImpl::abandon()
{

    hlog(HLOG_DEBUG, "abandoning process ID %u", mProcessPid);

    if (mManagementChannel)
    {
        getProcessManager()->abandonProcess(mManagementChannel);
    }

    setState(STATE_ABANDONED);
}

bool Forte::ProcessFutureImpl::IsRunning()
{
    return isInRunningState();
}

void Forte::ProcessFutureImpl::Signal(int signum)
{
    if (!isInRunningState())
        throw EProcessFutureNotRunning();

    PDUPtr pdu(new PDU(ProcessOpControlReq, sizeof(ProcessControlReqPDU)));
    ProcessControlReqPDU *control = pdu->GetPayload<ProcessControlReqPDU>();

    control->control = ProcessControlSignal;
    control->signum = signum;
    mManagementChannel->EnqueuePDU(pdu);
}

unsigned int Forte::ProcessFutureImpl::GetStatusCode()
{
    if(getState() == STATE_READY)
    {
        hlog(HLOG_ERR, "tried grabbing the status code from a process that hasn't been started");
        throw EProcessFutureNotStarted();
    }
    else if (!isInTerminalState())
    {
        hlog(HLOG_ERR, "tried grabbing the status code from a process that hasn't completed yet");
        throw EProcessFutureNotFinished();
    }
    return mStatusCode;
}

Forte::ProcessFuture::ProcessTerminationType Forte::ProcessFutureImpl::GetProcessTerminationType()
{
    int state = getState();

    if(state == STATE_READY)
    {
        hlog(HLOG_ERR, "tried grabbing the termination type from a process that hasn't been started");
        throw EProcessFutureNotStarted();
    }
    else if (!isInTerminalState())
    {
        hlog(HLOG_ERR, "tried grabbing the termination type from a process that hasn't completed yet");
        throw EProcessFutureNotFinished();
    }
    else if (state == STATE_EXITED)
        return ProcessExited;
    else if (state == STATE_KILLED)
        return ProcessKilled;
    else // STATE_ERROR or other
        return ProcessUnknownTermination;
}

FString Forte::ProcessFutureImpl::GetOutputString()
{
    if(getState() == STATE_READY)
    {
        hlog(HLOG_ERR, "tried grabbing the output from a process that hasn't been started");
        throw EProcessFutureNotStarted();
    }
    else if(!isInTerminalState())
    {
        hlog(HLOG_ERR, "tried grabbing the output from a process that hasn't completed yet");
        throw EProcessFutureNotFinished();
    }

    // lazy loading of the output string
    // check to see if the output string is empty
    // if so, and the output file wasn't the bit bucket
    // load it up and return it. Otherwise, we just load the
    // string we have already loaded
    if (mOutputString.empty() && mOutputFilename != "/dev/null")
    {
        // read log file
        FString stmp;
        ifstream in(mOutputFilename, ios::in | ios::binary);
        char buf[4096];

        mOutputString.clear();

        while (in.good())
        {
            in.read(buf, sizeof(buf));
            stmp.assign(buf, in.gcount());
            mOutputString += stmp;
        }

        // cleanup
        in.close();
    }
    else if(mOutputFilename == "/dev/null")
    {
        hlog(HLOG_WARN, "no output filename set");
        mOutputString.clear();
    }

    return mOutputString;
}

FString Forte::ProcessFutureImpl::GetErrorString()
{
    if(getState() == STATE_READY)
    {
        hlog(HLOG_ERR, "tried grabbing the error from a process that hasn't been started");
        throw EProcessFutureNotStarted();
    }
    else if(!isInTerminalState())
    {
        hlog(HLOG_ERR, "tried grabbing the error from a process that hasn't completed yet");
        throw EProcessFutureNotFinished();
    }

    // lazy loading of the error string
    // check to see if the error string is empty
    // if so, and the error file wasn't the bit bucket
    // load it up and return it. Otherwise, we just load the
    // string we have already loaded
    if (mErrorString.empty() && mErrorFilename != "/dev/null")
    {
        // read log file
        FString stmp;
        ifstream in(mErrorFilename, ios::in | ios::binary);
        char buf[4096];

        mErrorString.clear();

        while (in.good())
        {
            in.read(buf, sizeof(buf));
            stmp.assign(buf, in.gcount());
            mErrorString += stmp;
        }

        // cleanup
        in.close();
    }
    else if(mErrorFilename == "/dev/null")
    {
        hlog(HLOG_WARN, "no error filename set");
        mErrorString.clear();
    }

    return mErrorString;
}

void Forte::ProcessFutureImpl::Cancel()
{
    Signal(SIGTERM);
    Forte::Future<void>::Cancel();
}

void Forte::ProcessFutureImpl::handlePDU(PDUPeer &peer)
{
    FTRACE;
    PDU pdu;
    while (peer.RecvPDU(pdu))
    {
        hlog(HLOG_DEBUG, "PDU opcode %d", pdu.GetOpcode());
        switch (pdu.GetOpcode())
        {
        case ProcessOpControlRes:
            handleControlRes(peer, pdu);
            break;
        case ProcessOpStatus:
            handleStatus(peer, pdu);
            break;
        default:
            hlog(HLOG_ERR, "unexpected PDU with opcode %d", pdu.GetOpcode());
            break;
        }
    }
}

void Forte::ProcessFutureImpl::handleControlRes(PDUPeer &peer, const PDU &pdu)
{
    FTRACE;
    const ProcessControlResPDU *resPDU = pdu.GetPayload<ProcessControlResPDU>();
    hlog(HLOG_DEBUG, "got back result code %d", resPDU->result);
    mMonitorPid = resPDU->monitorPID;
    mProcessPid = resPDU->processPID;
    mStatusCode = resPDU->result;
    if (resPDU->result == ProcessSuccess)
        setState(STATE_RUNNING);
    else
    {
        mErrorString.assign(resPDU->error);
        setState(STATE_ERROR);
    }
}

void Forte::ProcessFutureImpl::handleStatus(PDUPeer &peer, const PDU &pdu)
{
    FTRACE;
    const ProcessStatusPDU *status = pdu.GetPayload<ProcessStatusPDU>();
    hlog(HLOG_DEBUG, "got back status type %d code %d", status->type, status->statusCode);
    mStatusCode = status->statusCode;
    switch (status->type)
    {
    case ProcessStatusStarted:
        setState(STATE_RUNNING);
        break;
    case ProcessStatusError:
        setState(STATE_ERROR);
        break;
    case ProcessStatusExited:
        setState(STATE_EXITED);
        break;
    case ProcessStatusKilled:
        setState(STATE_KILLED);
        break;
    case ProcessStatusStopped:
        setState(STATE_STOPPED);
        break;
    default:
        hlog(HLOG_ERR, "unknown status type!");
        break;
    }
}

void Forte::ProcessFutureImpl::handleError(PDUPeer &peer)
{
    // the process monitor connection has encountered an unrecoverable
    // error.
    hlog(HLOG_ERR, "lost connection to procmon");
    if (!isInTerminalState())
    {
        mStatusCode = ProcessProcmonFailure;
        setState(STATE_ERROR);
    }
}
