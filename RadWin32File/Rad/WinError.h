#ifndef __WINDOWSERROR_H__
#define __WINDOWSERROR_H__

/*
    Windows Error

    To be used like an exception.  When an error occurs just ThrowWinError().  Higher up in
    the program it can be caught converted to a string and displayed to the user.

    Errors from <WinError.H>
*/

#include <Windows.h>
#include <string>

#include "tstring.h"

namespace rad
{
    class WinError
    {
    private:
        DWORD           m_Error;
        std::tstring    m_UserMessage;
        HMODULE         m_hModule;

    public:
        WinError(HMODULE hModule, const std::tstring& UserMessage = _T("Windows Error : "), DWORD Error = GetLastError())
            : m_Error(Error), m_UserMessage(UserMessage), m_hModule(hModule)
        {
        }

        WinError(const std::tstring& UserMessage = _T("Windows Error : "), DWORD Error = GetLastError())
            : m_Error(Error), m_UserMessage(UserMessage), m_hModule(NULL)
        {
        }

        DWORD GetError() const
        {
            return m_Error;
        }

        std::tstring GetString() const { return m_UserMessage + GetString(m_Error, m_hModule); }
        static std::tstring GetString(DWORD error, HMODULE hModule = NULL);
    };

    inline void ThrowWinError(const WinError& error = WinError())
    {
        throw error;
    }

    inline void ThrowWinError(LPCTSTR msg)
    {
        throw WinError(msg);
    }
}

#endif //__WINDOWSERROR_H__
