#pragma once
#ifdef _MSC_VER
#include <windows.h>

inline void SetThreadName(const char* threadName)
{
#pragma pack(push, 8)

    struct THREADNAME_INFO
    {
        DWORD  dwType;        // Must be 0x1000.
        LPCSTR szName;        // Pointer to name (in user addr space).
        DWORD  dwThreadID;    // Thread ID (-1=caller thread).
        DWORD  dwFlags;       // Reserved for future use, must be zero.
    };
#pragma pack(pop)

    DWORD dwThreadID = GetCurrentThreadId();

    THREADNAME_INFO info;
    info.dwType     = 0x1000;
    info.szName     = threadName;
    info.dwThreadID = dwThreadID;
    info.dwFlags    = 0;

    __try
    {
        RaiseException(/*MS_VC_EXCEPTION*/ 0x406D1388, 0, sizeof(info) / sizeof(ULONG_PTR), (ULONG_PTR*)&info);
    } __except (EXCEPTION_EXECUTE_HANDLER)
    {}
}

#elif defined(__linux__)
#include <sys/prctl.h>
inline void SetThreadName(const char* threadName)
{
    prctl(PR_SET_NAME, threadName, 0, 0, 0);
}

#else
#include <pthread.h>
inline void SetThreadName(const char* threadName)
{
    pthread_setname_np(pthread_self(), threadName);
}
#endif
