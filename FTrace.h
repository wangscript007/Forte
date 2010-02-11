#ifndef __forte_Trace_h__
#define __forte_Trace_h__

#include "ThreadKey.h"
#include <list>
#include <map>

// maximum depth for which tracing and profiling data is maintained:
#define FTRACE_MAXDEPTH 256

class FProfileData {
public:
    FProfileData(void *fn, const struct timeval &spent, const struct timeval &totalSpent) 
        __attribute__ ((no_instrument_function));
    void addCall(const struct timeval &spent, const struct timeval &totalSpent) 
        __attribute__ ((no_instrument_function));

    const void *mFunction;
    unsigned int mCalls;
    struct timeval mSpent;
    struct timeval mTotalSpent;
    std::list<void*> mStackSample;
};

class FTraceThreadInfo {
public:
    __attribute__ ((no_instrument_function)) FTraceThreadInfo();
    __attribute__ ((no_instrument_function)) ~FTraceThreadInfo();
    unsigned short depth;
    bool profiling;
    void *stack[FTRACE_MAXDEPTH];
    void *lastfn[FTRACE_MAXDEPTH];
    void *fn;

    // profile data (if enabled):
    struct timeval entry[FTRACE_MAXDEPTH];
    struct timeval reentry[FTRACE_MAXDEPTH];
    struct timeval spent[FTRACE_MAXDEPTH];
    
    void storeProfile(void *fn, struct timeval &spent, struct timeval &totalSpent) 
        __attribute__ ((no_instrument_function));
    std::map<void*,FProfileData *> profileData;
};

class FTrace {
public:
    static void enable() __attribute__ ((no_instrument_function));
    static void disable() __attribute__ ((no_instrument_function));
    static void setProfiling(bool profile) __attribute__ ((no_instrument_function));
    static void dumpProfiling(unsigned int num = 0) __attribute__ ((no_instrument_function));
    static void enter(void *fn, void *caller) __attribute__ ((no_instrument_function));
    static void exit(void *fn, void *caller) __attribute__ ((no_instrument_function));
    static void cleanup(void *ptr) __attribute__ ((no_instrument_function));

    static unsigned int getDepth(void) __attribute__ ((no_instrument_function));
    static void getStack(std::list<void *> &stack /* OUT */) __attribute__ ((no_instrument_function));
    static FString & formatStack(const std::list<void *> &stack, FString &out) __attribute__ ((no_instrument_function));

    static unsigned int sInitialized;
protected:
    static FTraceThreadInfo* getThreadInfo(void) __attribute__ ((no_instrument_function));
    static CThreadKey sTraceKey;
    static FTrace *sInstance;
};

#endif
