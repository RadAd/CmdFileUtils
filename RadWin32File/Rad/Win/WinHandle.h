#ifndef WINHANDLE_H
#define WINHANDLE_H

#include <Rad/WinError.h>
#include <memory>

class WinHandleDeleter
{
public:
    typedef HANDLE pointer;
    void operator()(HANDLE Handle)
    {
        if (Handle != 0 && Handle != INVALID_HANDLE_VALUE)
        {
            if (::CloseHandle(Handle) == 0)
                rad::ThrowWinError();
        }
    }
};

class CWinHandle : private std::unique_ptr<HANDLE, WinHandleDeleter>
{
public:
    CWinHandle(HANDLE Handle = INVALID_HANDLE_VALUE)
        : std::unique_ptr<HANDLE, WinHandleDeleter>(Handle)
    {
    }

    void Close()
    {
        release();
    }

    HANDLE Get() const
    {
        return get();
    }

    void Attach(HANDLE Handle = INVALID_HANDLE_VALUE)
    {
        operator=(Handle);
    }

    HANDLE Release()
    {
        return release();
    }
};

#endif
