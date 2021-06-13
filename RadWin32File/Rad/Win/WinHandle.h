#ifndef WINHANDLE_H
#define WINHANDLE_H

#include "../WinError.h"
#include <memory>

namespace rad
{
    inline BOOL __stdcall CheckCloseHandle(HANDLE Handle)
    {
        if (Handle != NULL && Handle != INVALID_HANDLE_VALUE)
        {
            if (!::CloseHandle(Handle))
                ThrowWinError(_T(__FUNCTION__));
        }
        return TRUE;
    }

    template <class T = HANDLE>
    class WinHandle : private std::unique_ptr<typename std::remove_pointer<T>::type, BOOL(__stdcall *)(T)>
    {
    public:
        WinHandle(T Handle = INVALID_HANDLE_VALUE, BOOL(__stdcall *func)(T) = CheckCloseHandle)
            : std::unique_ptr<std::remove_pointer<T>::type, BOOL(__stdcall *)(T)>(Handle, func)
        {
        }

        void Close()
        {
            release();
        }

        T Get() const
        {
            return get();
        }

        void Attach(T Handle = INVALID_HANDLE_VALUE)
        {
            operator=(Handle);
        }

        T Release()
        {
            return release();
        }

        explicit operator bool() const noexcept
        {   // test for non-null pointer
            return (get() != NULL && get() != INVALID_HANDLE_VALUE);
        }
    };
}

#endif
