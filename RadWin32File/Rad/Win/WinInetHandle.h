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

class CWinInetHandle
{
public:
    CWinInetHandle(HINTERNET Handle = NULL)
        : m_Handle(Handle)
    {
    }

    ~CWinInetHandle()
    {
        Close();
    }

    void Close()
    {
        CloseHandle(Release());
    }

    CWinInetHandle & operator = (CWinInetHandle &Other)
    {
        Close();
        m_Handle = Other.Release();
        return *this;
    }

    HINTERNET Get() const
    {
        return m_Handle;
    }

    void Attach(HINTERNET Handle = NULL)
    {
        Close();
        m_Handle = Handle;
    }

    HINTERNET Release()
    {
        HINTERNET	Handle = m_Handle;
        m_Handle = NULL;
        return Handle;
    }

    static void CloseHandle(HINTERNET Handle)
    {
        if (Handle != NULL)
        {
            if (::InternetCloseHandle(Handle) == 0)
                ThrowWinInetError();
        }
    }

private:
    HINTERNET	m_Handle;
};

#endif
