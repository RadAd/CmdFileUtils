#ifndef WININETHANDLE_H
#define WININETHANDLE_H

#include <Rad/WinError.h>
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

inline void CheckInternetCloseHandle(HANDLE Handle)
{
    if (Handle != NULL)
    {
        if (!::InternetCloseHandle(Handle))
            ThrowWinInetError();
    }
}

class CWinInetHandle : private std::unique_ptr<HINTERNET, rad::WinHandleDeleter<HINTERNET, CheckInternetCloseHandle>>
{
public:
    CWinInetHandle(HINTERNET Handle = NULL)
        : std::unique_ptr<HINTERNET, rad::WinHandleDeleter<HINTERNET, CheckInternetCloseHandle>>(Handle)
    {
    }

    void Close()
    {
        release();
    }

    HINTERNET Get() const
    {
        return get();
    }

    void Attach(HINTERNET Handle = NULL)
    {
        operator=(Handle);
    }

    HINTERNET Release()
    {
        return release();
    }
};

#endif
