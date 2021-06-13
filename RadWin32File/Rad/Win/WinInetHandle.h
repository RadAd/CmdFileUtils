#ifndef WININETHANDLE_H
#define WININETHANDLE_H

#include "WinError.h"
#include "WinHandle.h"
#include <WinInet.H>

inline void ThrowWinInetError()
{
    rad::WinError error(GetModuleHandle(TEXT("wininet.dll")));
    if (error.GetError() == 12003) // The server returned extended information
    {
        DWORD e;
        TCHAR Buffer[1024];
        DWORD s = 1024;
        InternetGetLastResponseInfo(&e, Buffer, &s);
        rad::ThrowWinError(rad::WinError(GetModuleHandle(TEXT("wininet.dll")), Buffer));
    }
    else
        ThrowWinError(error);
}

inline void ThrowWinInetError(LPCTSTR msg)
{
    rad::WinError error(GetModuleHandle(TEXT("wininet.dll")), msg);
    if (error.GetError() == 12003) // The server returned extended information
    {
        DWORD e;
        TCHAR Buffer[1024];
        DWORD s = 1024;
        InternetGetLastResponseInfo(&e, Buffer, &s);
        rad::ThrowWinError(rad::WinError(GetModuleHandle(TEXT("wininet.dll")), Buffer));
    }
    else
        ThrowWinError(error);
}

inline BOOL __stdcall CheckInternetCloseHandle(HINTERNET Handle)
{
    if (Handle != NULL)
    {
        if (!::InternetCloseHandle(Handle))
            ThrowWinInetError();
    }
    return TRUE;
}

class CWinInetHandle : public rad::WinHandle<HINTERNET>
{
public:
    CWinInetHandle(HINTERNET Handle = NULL)
        : rad::WinHandle<HINTERNET>(Handle, CheckInternetCloseHandle)
    {
    }

    void Attach(HINTERNET Handle = NULL)
    {
        operator=(Handle);
    }
};

#endif
