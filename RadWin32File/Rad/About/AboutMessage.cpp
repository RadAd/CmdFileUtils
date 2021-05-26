#include "AboutMessage.H"

#include <Rad/tstring.h>
#include <cstdio>
#include <io.h>
#include <tchar.h>

#define ESC TEXT("\x1B")
#define ANSI_COLOR ESC TEXT("[%sm")
#define ANSI_COLOR_(x) ESC TEXT("[") TEXT(#x) TEXT("m")
#define ANSI_RESET ESC TEXT("[0m")

void DisplayAboutMessage(HINSTANCE hInstance, const TCHAR *Product)
{
    const bool Ansi = _isatty(_fileno(stdout));
    TCHAR	FileVersion[1024];

    TCHAR	FileName[1024];
    GetModuleFileName(hInstance, FileName, 1024);

    DWORD	Size = GetFileVersionInfoSize(FileName, nullptr);

    if (Size > 0)
    {
        void	*Info = malloc(Size);
        if (Info != nullptr)
        {
            // VS_VERSION_INFO   VS_VERSIONINFO  VS_FIXEDFILEINFO

            GetFileVersionInfo(FileName, 0, Size, Info);

            TCHAR*  Version = nullptr;
            UINT    Length;
            VerQueryValue(Info, TEXT("\\StringFileInfo\\0c0904b0\\FileVersion"), (LPVOID*) &Version, &Length);

            _tcscpy_s(FileVersion, Version);

            free(Info);
        }
    }
    else
    {
        _tcscpy_s(FileVersion, TEXT("Unknown"));
    }

    if (Ansi)
        _tprintf(ANSI_COLOR_(37) TEXT("%s") ANSI_RESET TEXT(" Version") ANSI_COLOR_(37) TEXT(" %s\n") ANSI_RESET, Product, FileVersion);
    else
        _tprintf(TEXT("%s Version %s\n") ANSI_RESET, Product, FileVersion);
    _tprintf(TEXT(" By Adam Gates (adamgates84+software@gmail.com) - https://github.com/RadAd/CmdFileUtils\n"));
}
