#ifndef _Forte_Process_Manager_PDU_h_
#define _Forte_Process_Manager_PDU_h_

namespace Forte
{

    enum PDUOpcodes
    {
        ProcessOpPrepare,
        ProcessOpStatus,
        ProcessOpOutput,
        ProcessOpControlReq,
        ProcessOpControlRes,
        ProcessOpInfoReq,
        ProcessOpInfoRes
    };

    enum ProcessStatusType
    {
        ProcessStatusStarted,
        ProcessStatusError,
        ProcessStatusExited,
        ProcessStatusKilled,
        ProcessStatusStopped,
        ProcessStatusUnknownTermination,
        ProcessStatusNotTerminated
    };

    struct ProcessPreparePDU
    {
        char cmdline[2048];
        char cwd[1024];
    } __attribute__((__packed__));
    
    struct ProcessStatusPDU
    {
        int type;   // started, error, exited, killed, stopped
        int statusCode; // exit code, signal # delivered, or error type
        struct timeval timestamp;
        int msgLen;
        char msg[1024];
    } __attribute__((__packed__));
    struct ProcessOutputPDU
    {
        int len;
        char data[1024];
    } __attribute__((__packed__));

    enum ProcessControlCodes
    {
        ProcessControlStart,
        ProcessControlSignal
    };
    struct ProcessControlReqPDU
    {
        int control;   // start, signal
        int signum;
    } __attribute__((__packed__));

    enum ProcessControlResultCodes
    {
        ProcessSuccess,
        ProcessUnableToOpenInputFile,
        ProcessUnableToOpenOutputFile,
        ProcessUnableToFork,
        ProcessNotRunning,
        ProcessRunning,
        ProcessUnknownError
    };
    struct ProcessControlResPDU
    {
        int result; // 0 = success
        int monitorPID;
        int processPID;
        char error[1024]; // description of error
    } __attribute__((__packed__));
    struct ProcessInfoReqPDU
    {
        int nothing;
    };
    struct ProcessInfoResPDU
    {
        // started by process name (short name)
        char startedBy[64];
        // started by PID
        int startedByPID;
        // start timestamp
        struct timeval startTime;
        // elapsed time
        struct timeval elapsed;
        // command line
        char cmdline[2048];
        // cwd
        char cwd[1024];
        // monitor PID
        int monitorPID;
        // process PID
        int processPID;
    } __attribute__((__packed__));
};
#endif
