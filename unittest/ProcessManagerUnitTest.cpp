#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "LogManager.h"
#include "ProcessManagerImpl.h"
#include "ProcessFuture.h"
#include "Future.h"
#include "AutoFD.h"
#include "FileSystem.h"
#include "ProcFileSystem.h"
#include "AutoDoUndo.h"
#include <boost/bind.hpp>
#include <boost/make_shared.hpp>

using namespace Forte;

LogManager logManager;

static FString mProcMonLocation;

class ProcessManagerTest : public ::testing::Test
{
public:
    static void SetUpTestCase() {
        signal(SIGPIPE, SIG_IGN);
        logManager.SetLogMask("//stdout", HLOG_ALL);
        logManager.BeginLogging("//stdout");

        FileSystem fs;
        // check all the possible dirs
        mProcMonLocation = "";
        if (fs.FileExists("../obj/SC001-x86_64-opt/procmon"))
        {
            mProcMonLocation.Format( 
                "FORTE_PROCMON=../obj/SC001-x86_64-%s/procmon", 
                "opt");            
        } 
        else if (fs.FileExists("../obj/SC001-x86_64-debug/procmon"))
        {
            mProcMonLocation.Format( 
                "FORTE_PROCMON=../obj/SC001-x86_64-%s/procmon", 
                "debug");            
        }
        else if (fs.FileExists("../obj/SC001-x86_64-rel/procmon"))
        {
            mProcMonLocation.Format( 
                "FORTE_PROCMON=../obj/SC001-x86_64-%s/procmon", 
                "rel");
        }
        else if (fs.FileExists("../obj/SC001-x86_64-prof/procmon"))
        {
            mProcMonLocation.Format( 
                "FORTE_PROCMON=../obj/SC001-x86_64-%s/procmon", 
                "prof");
        }
        // else let Processmanager use the default location it knows abt

        if (mProcMonLocation != "")
        {
            hlog(HLOG_INFO, "procmon location : %s", mProcMonLocation.c_str());
            putenv((char *)mProcMonLocation.c_str());
            hlog(HLOG_INFO, "procmon env : %s", getenv("FORTE_PROCMON"));
        }
    };
    static void TearDownTestCase() {

    };
    
};

TEST_F(ProcessManagerTest, Runs100ProcessesInUnder3Seconds)
{
    try
    {
        boost::shared_ptr<ProcessManager> pm(new ProcessManagerImpl);
        std::list<boost::shared_ptr<ProcessFuture> > phList;
        Forte::Timespec start = Forte::MonotonicClock().GetTime();
        for (int i = 0; i < 100; ++i)
        {
            phList.push_back(
                boost::shared_ptr<ProcessFuture>(
                    pm->CreateProcess("/bin/sleep 1")));
        }
        foreach(const boost::shared_ptr<Forte::ProcessFuture> &ph, phList)
        {
            ASSERT_NO_THROW(ph->GetResult());
            ASSERT_EQ(0, ph->GetStatusCode());
            ASSERT_EQ(ProcessFuture::ProcessExited, ph->GetProcessTerminationType());
        }
        Forte::Timespec finish = Forte::MonotonicClock().GetTime();
        ASSERT_TRUE(finish - start < Forte::Timespec::FromSeconds(3));
    }
    catch (std::exception& e)
    {
        hlog(HLOG_ERR, "exception: %s", e.what());
        FAIL();
    }
}
TEST_F(ProcessManagerTest, ReactsToChildTerminationInUnder50Millisec)
{
    try
    {
        boost::shared_ptr<ProcessManager> pm(new ProcessManagerImpl);
        boost::shared_ptr<ProcessFuture> ph(
            pm->CreateProcess("/bin/usleep 10"));
        Forte::Timespec start = Forte::MonotonicClock().GetTime();
        ASSERT_NO_THROW(ph->GetResult());
        Forte::Timespec finish = Forte::MonotonicClock().GetTime();
        ASSERT_EQ(0, ph->GetStatusCode());
        ASSERT_EQ(ProcessFuture::ProcessExited, ph->GetProcessTerminationType());
        ASSERT_TRUE(finish - start < Forte::Timespec::FromMillisec(50));
    }
    catch (std::exception& e)
    {
        hlog(HLOG_ERR, "exception: %s", e.what());
        FAIL();
    }
}
TEST_F(ProcessManagerTest, MemLeak)
{
    try
    {
        hlog(HLOG_INFO, "new ProcessManage");
        boost::shared_ptr<ProcessManager> pm(new ProcessManagerImpl);
        hlog(HLOG_INFO, "CreateProcess");
        
        sleep(1); // causes a race condition where the next Process is
        // added during the epoll_wait
        {
            hlog(HLOG_INFO, "Run Process");
            boost::shared_ptr<ProcessFuture> ph = pm->CreateProcess("/bin/sleep 3");
            hlog(HLOG_INFO, "Is Running");
            ASSERT_TRUE(ph->IsRunning());
            hlog(HLOG_INFO, "Wait");
            ASSERT_NO_THROW(ph->GetResult());
            hlog(HLOG_INFO, "Is Running");
            ASSERT_TRUE(!ph->IsRunning());
            hlog(HLOG_INFO, "Termination Type: %d", ph->GetProcessTerminationType());
            hlog(HLOG_INFO, "StatusCode: %d", ph->GetStatusCode());
            hlog(HLOG_INFO, "OutputString: %s", ph->GetOutputString().c_str());
        }
        // it is possible for the pf not to be destructed at this point due to a
        // pending PDU. We sleep here to let the destruction happen
	sleep(1);
        ASSERT_TRUE(pm->IsProcessMapEmpty());
    }
    catch (Exception& e)
    {
        hlog(HLOG_ERR, "exception: %s", e.what());
        FAIL();        
    }
}

TEST_F(ProcessManagerTest, FDLeak)
{
    try
    {
        hlog(HLOG_INFO, "new ProcessManager");

        Context c;
        // setup
        boost::shared_ptr<FileSystem> fsptr(new FileSystem());
        c.Set("forte.FileSystem", fsptr);

        ProcFileSystem pfs(c);

        boost::shared_ptr<ProcessManager> pm(new ProcessManagerImpl);
        hlog(HLOG_INFO, "CreateProcess");

        unsigned int openDescriptors = pfs.CountOpenFileDescriptors();
        hlog(HLOG_INFO, "Currently %u descriptors open", openDescriptors);
        sleep(1); // causes a race condition where the next Process is
        // added during the epoll_wait
        {
            hlog(HLOG_INFO, "Run Process");
            hlog(HLOG_INFO, "Creating First Process");
            boost::shared_ptr<ProcessFuture> ph1 = pm->CreateProcess("/bin/sleep 6");
            hlog(HLOG_INFO, "Open file descriptor : %u, expected: %u", 
                 pfs.CountOpenFileDescriptors(),
                 openDescriptors + 1);
            ASSERT_EQ(openDescriptors + 1, pfs.CountOpenFileDescriptors());

            hlog(HLOG_INFO, "Creating Second Process");
            boost::shared_ptr<ProcessFuture> ph2 = pm->CreateProcess("/bin/sleep 1");
            hlog(HLOG_INFO, "Open file descriptor : %u, expected: %u", 
                 pfs.CountOpenFileDescriptors(),
                 openDescriptors + 2);
            ASSERT_EQ(openDescriptors + 2, pfs.CountOpenFileDescriptors());

            hlog(HLOG_INFO, "Creating Third Process");
            boost::shared_ptr<ProcessFuture> ph3 = pm->CreateProcess("/bin/sleep 3");
            hlog(HLOG_INFO, "Open file descriptor : %u, expected: %u", 
                 pfs.CountOpenFileDescriptors(),
                 openDescriptors + 3);
            ASSERT_EQ(openDescriptors + 3, pfs.CountOpenFileDescriptors());

            ASSERT_NO_THROW(ph2->GetResult());
            ASSERT_FALSE(ph2->IsRunning());
            ph2.reset();
            sleep(1); // needed because Proc filsystem takes some time to remove the fd
            hlog(HLOG_INFO, "Open file descriptor : %u, expected: %u", 
                 pfs.CountOpenFileDescriptors(),
                 openDescriptors + 2);
            ASSERT_EQ(openDescriptors + 2, pfs.CountOpenFileDescriptors());

            ASSERT_NO_THROW(ph3->GetResult());
            ASSERT_FALSE(ph3->IsRunning());
            ph3.reset();
            sleep(1); // needed because Proc filsystem takes some time to remove the fd
            hlog(HLOG_INFO, "Open file descriptor : %u, expected: %u", 
                 pfs.CountOpenFileDescriptors(),
                 openDescriptors + 1);
            ASSERT_EQ(openDescriptors + 1, pfs.CountOpenFileDescriptors());

            ASSERT_NO_THROW(ph1->GetResult());
            ASSERT_FALSE(ph1->IsRunning());
            ph1.reset();
            sleep(1); // needed because Proc filsystem takes some time to remove the fd
            hlog(HLOG_INFO, "Open file descriptor : %u, expected: %u", 
                 pfs.CountOpenFileDescriptors(),
                 openDescriptors);
            ASSERT_EQ(openDescriptors, pfs.CountOpenFileDescriptors());
        }
        ASSERT_TRUE(pm->IsProcessMapEmpty());

        ASSERT_EQ(openDescriptors, pfs.CountOpenFileDescriptors());

    }
    catch (Exception& e)
    {
        hlog(HLOG_ERR, "exception: %s", e.what());
        FAIL();
    }
}

TEST_F(ProcessManagerTest, RunProcess)
{
    try
    {
        hlog(HLOG_INFO, "new ProcessManager");
        boost::shared_ptr<ProcessManager> pm(new ProcessManagerImpl);
        hlog(HLOG_INFO, "CreateProcess");

        sleep(1); // causes a race condition where the next Process is
                  // added during the epoll_wait

        hlog(HLOG_INFO, "Run Process");
        boost::shared_ptr<ProcessFuture> ph = pm->CreateProcess("/bin/sleep 3");
        hlog(HLOG_INFO, "Is Running");
        ASSERT_TRUE(ph->IsRunning());
        hlog(HLOG_INFO, "Wait");
        ASSERT_NO_THROW(ph->GetResult());
        hlog(HLOG_INFO, "Is Running");
        ASSERT_TRUE(!ph->IsRunning());
        hlog(HLOG_INFO, "Termination Type: %d", ph->GetProcessTerminationType());
        hlog(HLOG_INFO, "StatusCode: %d", ph->GetStatusCode());
        hlog(HLOG_INFO, "OutputString: %s", ph->GetOutputString().c_str());
    }
    catch (Exception &e)
    {
        hlog(HLOG_ERR, "exception: %s", e.what());
        FAIL();
    }
}

void cleanupOutputFile(const Forte::FString &filename)
{
    unlink(filename.c_str());
}

TEST_F(ProcessManagerTest, ErrorOutput)
{
    try
    {
        hlog(HLOG_INFO, "new ProcessManager");
        boost::shared_ptr<ProcessManager> pm(new ProcessManagerImpl);
        hlog(HLOG_INFO, "CreateProcess");

        sleep(1); // causes a race condition where the next Process is
                  // added during the epoll_wait
        AutoDoUndo<void, void> autoCleanup(
            boost::function<void ()>(),
            boost::bind(&cleanupOutputFile, "/tmp/pmuinttesterrorout.tmp"),
            false);

        hlog(HLOG_INFO, "Run Process");
        boost::shared_ptr<ProcessFuture> ph = pm->CreateProcess(
            "/bin/echo Error: file already exists. >&2", "/", 
            "/dev/null", "/tmp/pmuinttesterrorout.tmp");

        ASSERT_NO_THROW(ph->GetResult());
        hlog(HLOG_INFO, "Is Running");
        ASSERT_TRUE(!ph->IsRunning());
        ASSERT_TRUE(ph->GetErrorString() == "Error: file already exists.\n");
    }
    catch (Exception &e)
    {
        hlog(HLOG_ERR, "exception: %s", e.what());
        FAIL();
    }
}

TEST_F(ProcessManagerTest, ErrorOutputNonZero)
{
    try
    {
        hlog(HLOG_INFO, "new ProcessManager");
        boost::shared_ptr<ProcessManager> pm(new ProcessManagerImpl);
        hlog(HLOG_INFO, "CreateProcess");

        sleep(1); // causes a race condition where the next Process is
                  // added during the epoll_wait
        AutoDoUndo<void, void> autoCleanup(
            boost::function<void ()>(),
            boost::bind(&cleanupOutputFile, "/tmp/pmuinttesterrorout.tmp"),
            false);

        hlog(HLOG_INFO, "Run Process");
        boost::shared_ptr<ProcessFuture> ph = pm->CreateProcess(
            "/bin/echo Error: file already exists. >&2; /bin/false", "/", 
            "/dev/null", "/tmp/pmuinttesterrorout.tmp");

        ASSERT_THROW(ph->GetResult(), EProcessFutureTerminatedWithNonZeroStatus);
        ASSERT_TRUE(ph->GetErrorString() == "Error: file already exists.\n");
    }
    catch (Exception &e)
    {
        hlog(HLOG_ERR, "exception: %s", e.what());
        FAIL();
    }
}

TEST_F(ProcessManagerTest, ReturnCodeHandling)
{
    try
    {
        boost::shared_ptr<ProcessManager> pm(new ProcessManagerImpl);
        boost::shared_ptr<ProcessFuture> ph = pm->CreateProcess("/bin/false");
        ASSERT_THROW(ph->GetResult(), EProcessFutureTerminatedWithNonZeroStatus);
        ASSERT_TRUE(!ph->IsRunning());
        hlog(HLOG_INFO, "Termination Type: %d", ph->GetProcessTerminationType());
        hlog(HLOG_INFO, "StatusCode: %d", ph->GetStatusCode());
        hlog(HLOG_INFO, "OutputString: %s", ph->GetOutputString().c_str());
        ASSERT_EQ(ph->GetProcessTerminationType(), Forte::ProcessFuture::ProcessExited);
        ASSERT_EQ(ph->GetStatusCode(), 1);
    }
    catch (Exception &e)
    {
        hlog(HLOG_ERR, "exception: %s", e.what());
        FAIL();
    }
}


TEST_F(ProcessManagerTest, RunMultipleProcess)
{
    try
    {
        boost::shared_ptr<ProcessManager> pm(new ProcessManagerImpl);
        
        sleep(3);

        boost::shared_ptr<ProcessFuture> ph1 = pm->CreateProcess("/bin/sleep 3");
        boost::shared_ptr<ProcessFuture> ph2 = pm->CreateProcess("/bin/sleep 5");
        ASSERT_TRUE(ph1->IsRunning());
        ASSERT_TRUE(ph2->IsRunning());
        ASSERT_NO_THROW(ph1->GetResult());
        ASSERT_NO_THROW(ph2->GetResult());
        ASSERT_TRUE(!ph1->IsRunning());
        ASSERT_TRUE(!ph2->IsRunning());
    }
    catch (Exception &e)
    {
        hlog(HLOG_ERR, "exception: %s", e.what());
        FAIL();
    }
}

TEST_F(ProcessManagerTest, Exceptions)
{
    try
    {
        hlog(HLOG_INFO, "Exceptions");
        boost::shared_ptr<ProcessManager> pm(new ProcessManagerImpl);
        hlog(HLOG_INFO, "CreateProcess");
        boost::shared_ptr<ProcessFuture> ph = pm->CreateProcessDontRun("/bin/sleep 3");
        hlog(HLOG_INFO, "GetProcessTerminationType");
        ASSERT_THROW(ph->GetProcessTerminationType(), EProcessFutureNotStarted);
        hlog(HLOG_INFO, "GetStatusCode");
        ASSERT_THROW(ph->GetStatusCode(), EProcessFutureNotStarted);    
        hlog(HLOG_INFO, "GetOutputString");
        ASSERT_THROW(ph->GetOutputString(), EProcessFutureNotStarted);

        hlog(HLOG_INFO, "Wait");
        ASSERT_THROW(ph->GetResult(), EProcessFutureNotRunning);

        pm->RunProcess(ph);
        ASSERT_THROW(ph->GetProcessTerminationType(), EProcessFutureNotFinished);
        ASSERT_THROW(ph->GetStatusCode(), EProcessFutureNotFinished);    
        ASSERT_THROW(ph->GetOutputString(), EProcessFutureNotFinished);

        ASSERT_THROW(ph->SetProcessCompleteCallback(NULL), EProcessFutureStarted);
        ASSERT_THROW(ph->SetCurrentWorkingDirectory(""), EProcessFutureStarted);
        ASSERT_THROW(ph->SetEnvironment(NULL), EProcessFutureStarted);
        ASSERT_THROW(ph->SetInputFilename(""), EProcessFutureStarted);
        ASSERT_THROW(ph->SetOutputFilename(""), EProcessFutureStarted);
        ASSERT_THROW(pm->RunProcess(ph), EProcessFutureStarted);

        ph->GetResult();
    }
    catch (Exception &e)
    {
        hlog(HLOG_ERR, "Exception: %s", e.what());
        FAIL();
    }
    catch (std::exception &e)
    {
        hlog(HLOG_ERR, "std::exception: %s", e.what());
        FAIL();
    }
}

TEST_F(ProcessManagerTest, CancelProcess)
{
    try
    {
        hlog(HLOG_INFO, "CancelProcess");
        boost::shared_ptr<ProcessManager> pm(new ProcessManagerImpl);
        boost::shared_ptr<ProcessFuture> ph = pm->CreateProcess("/bin/sleep 100");
        ASSERT_TRUE(ph->IsRunning());
        ph->Cancel();
        ASSERT_THROW(ph->GetResult(), EProcessFutureKilled);
        ASSERT_TRUE(ph->IsCancelled());
        ASSERT_TRUE(!ph->IsRunning());
        ASSERT_TRUE(ph->GetStatusCode() == SIGTERM);
        ASSERT_TRUE(ph->GetProcessTerminationType() == Forte::ProcessFuture::ProcessKilled);
        hlog(HLOG_INFO, "OutputString: %s", ph->GetOutputString().c_str());
    }
    catch (Exception &e)
    {
        hlog(HLOG_ERR, "Exception: %s", e.what());
        FAIL();
    }
    catch (std::exception &e)
    {
        hlog(HLOG_ERR, "std::exception: %s", e.what());
        FAIL();
    }    
}


TEST_F(ProcessManagerTest, AbandonProcess)
{
    try
    {
        pid_t pid = -1;
        hlog(HLOG_INFO, "AbandonProcess");
        {
            boost::shared_ptr<ProcessManager> pm(new ProcessManagerImpl);
            boost::shared_ptr<ProcessFuture> ph = pm->CreateProcess("/bin/sleep 100");
            pid = ph->GetProcessPID();
            ASSERT_TRUE(ph->IsRunning());
        }
        // make sure the process still exists:
        ASSERT_TRUE(kill(pid, 0) == 0);
    }
    catch (Exception &e)
    {
        hlog(HLOG_ERR, "Exception: %s", e.what());
        FAIL();
    }
    catch (std::exception &e)
    {
        hlog(HLOG_ERR, "std::exception: %s", e.what());
        FAIL();
    }
}

TEST_F(ProcessManagerTest, FileIO)
{
    try
    {
        hlog(HLOG_INFO, "FileIO");
        boost::shared_ptr<ProcessManager> pm(new ProcessManagerImpl);
        boost::shared_ptr<ProcessFuture> ph = pm->CreateProcessDontRun("/bin/sleep 1");

        // check that we propery throw when the input file doesn't exist
        ph->SetInputFilename("/foo/bar/baz");
        pm->RunProcess(ph);
        ASSERT_THROW(ph->GetResult(), EProcessFutureUnableToOpenInputFile);

        // check that we properly throw when the output file can't be created
        ph = pm->CreateProcessDontRun("/bin/sleep");
        ph->SetOutputFilename("/foo/bar/baz");
        pm->RunProcess(ph);
        ASSERT_THROW(ph->GetResult(), EProcessFutureUnableToOpenOutputFile);
        

        hlog(HLOG_DEBUG, "getting ready to try some output");

        // check that when we can create an output file that we can get 
        // the contents
        boost::shared_ptr<ProcessFuture> ph2 = pm->CreateProcess("/bin/ls", "/", "temp.out");
        ph2->GetResult();
        ASSERT_TRUE(ph2->GetOutputString().find("proc") != string::npos);

        hlog(HLOG_DEBUG, "okay, now what happens when output is to /dev/null");

        // what happens if /dev/null is our output?
        boost::shared_ptr<ProcessFuture> ph3 = pm->CreateProcess("/bin/ls", "/");
        ph3->GetResult();
        ASSERT_TRUE(ph3->GetOutputString().find("proc") == string::npos);

        hlog(HLOG_DEBUG, "reached end of function");
    }
    catch (Exception &e)
    {
        hlog(HLOG_ERR, "Exception: %s", e.what());
        FAIL();
    }
    catch (std::exception &e)
    {
        hlog(HLOG_ERR, "std::exception: %s", e.what());
        FAIL();
    }
}

bool complete = false;

void ProcessComplete(boost::shared_ptr<ProcessFuture> ph)
{
    hlog(HLOG_DEBUG, "process completion callback triggered");
    ASSERT_TRUE(ph->GetOutputString().find("proc") != string::npos);
    complete = true;
}

TEST_F(ProcessManagerTest, Callbacks)
{
    try
    {
        hlog(HLOG_INFO, "Callbacks");
        boost::shared_ptr<ProcessManager> pm(new ProcessManagerImpl);
        boost::shared_ptr<ProcessFuture> ph = pm->CreateProcessDontRun("/bin/ls", "/", "temp.out");
        ph->SetProcessCompleteCallback(ProcessComplete);
        pm->RunProcess(ph);
        ph->GetResult();
        ASSERT_TRUE(complete);
    }
    catch (Exception &e)
    {
        hlog(HLOG_ERR, "Exception: %s", e.what());
        FAIL();
    }
    catch (std::exception &e)
    {
        hlog(HLOG_ERR, "std::exception: %s", e.what());
        FAIL();
    }
}

TEST_F(ProcessManagerTest, ProcmonRunning)
{
    try
    {
        hlog(HLOG_INFO, "ProcmonRunning");
        pid_t monitorPid = -1;
        {
            boost::shared_ptr<ProcessManager> pm(new ProcessManagerImpl);
            boost::shared_ptr<ProcessFuture> ph = pm->CreateProcess("/bin/sleep 3");
            ph->GetResult();
            monitorPid = ph->GetMonitorPID();
        }

        sleep(5); // wait for procmon to finish

        ASSERT_TRUE(monitorPid != -1);
        hlog(HLOG_INFO, "Going to check if monitor process %d is still running", monitorPid);
        // check if the monitor pid is still running
        FString filename(FStringFC(), 
                         "/proc/%d/cmdline", monitorPid);
        hlog(HLOG_INFO, "monitor proc location %s", filename.c_str());
        AutoFD fd;
        fd = open(filename, O_RDONLY);
        ASSERT_TRUE(fd < 0);
    }
    catch (Exception &e)
    {
        hlog(HLOG_ERR, "Exception: %s", e.what());
        FAIL();
    }
    catch (std::exception &e)
    {
        hlog(HLOG_ERR, "std::exception: %s", e.what());
        FAIL();
    }
}

TEST_F(ProcessManagerTest, ProcessTimeout)
{
    try
    {
        hlog(HLOG_INFO, "CancelProcess");
        boost::shared_ptr<ProcessManager> pm(new ProcessManagerImpl);
        boost::shared_ptr<ProcessFuture> ph = pm->CreateProcess("/bin/sleep 10");
        ASSERT_TRUE(ph->IsRunning());
        ASSERT_THROW(ph->GetResultTimed(Timespec::FromSeconds(5)), EFutureTimeoutWaitingForResult);
        ASSERT_NO_THROW(ph->GetResult());
        ASSERT_TRUE(!ph->IsRunning());
    }
    catch (Exception &e)
    {
        hlog(HLOG_ERR, "Exception: %s", e.what());
        FAIL();
    }
    catch (std::exception &e)
    {
        hlog(HLOG_ERR, "std::exception: %s", e.what());
        FAIL();
    }    
}

TEST_F(ProcessManagerTest, CommandLineEscape)
{
    try
    {
        hlog(HLOG_INFO, "Command Line Escapes");
        boost::shared_ptr<ProcessManager> pm(new ProcessManagerImpl);
        FString result;
        FString test("firstword secondword");
        pm->CreateProcessAndGetResult("/bin/echo \"" + test + "\"", result);
        result.Trim();
        ASSERT_STREQ(result, test);

        pm->CreateProcessAndGetResult("/bin/echo '" + test + "'", result);
        result.Trim();
        ASSERT_STREQ(result, test);

        pm->CreateProcessAndGetResult("/bin/echo \\'" + test + "\\'", result);
        result.Trim();
        ASSERT_STREQ(result, "'" + test + "'");

        pm->CreateProcessAndGetResult("/bin/echo \\\"" + test + "\\\"", result);
        result.Trim();
        ASSERT_STREQ(result, "\"" + test + "\"");
    }
    catch (Exception &e)
    {
        hlog(HLOG_ERR, "Exception: %s", e.what());
        FAIL();
    }
    catch (std::exception &e)
    {
        hlog(HLOG_ERR, "std::exception: %s", e.what());
        FAIL();
    }
}


TEST_F(ProcessManagerTest, ProcessManagerCanRunHost)
{
    using namespace boost;
    try
    {
        shared_ptr<ProcessManagerImpl> processManagerPtr = 
            make_shared<ProcessManagerImpl>();

        ProcessManagerImpl& processManager = *processManagerPtr;

        FString output;
        FString test("/usr/bin/host scalecomputing.com");
        processManager.CreateProcessAndGetResult(test, output);

        hlogstream(HLOG_INFO, "Got output:" << output);
    }
    catch (Exception &e)
    {
        hlogstream(HLOG_ERR, "Exception: " << 
                   typeid(e).name() << " " << e.what());
        FAIL();
    }
    catch (std::exception &e)
    {
        hlogstream(HLOG_ERR, "std_exception: " << 
                   typeid(e).name() << " " << e.what());
        FAIL();
    }
}
