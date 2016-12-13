#include "AboutMessage.H"
#include "../ConsoleUtils.h"

#include <Rad/tstring.h>
#include <stdio.h>
#include <tchar.h>

void DisplayAboutMessage(HINSTANCE hInstance, const TCHAR *Product)
{
    TCHAR	FileVersion[1024];

    TCHAR	FileName[1024];
    GetModuleFileName(hInstance, FileName, 1024);

    DWORD	Dummy;
    DWORD	Size = GetFileVersionInfoSize(FileName, &Dummy);

    if (Size > 0)
    {
        void	*Info = malloc(Size);

        // VS_VERSION_INFO   VS_VERSIONINFO  VS_FIXEDFILEINFO

        GetFileVersionInfo(FileName, Dummy, Size, Info);

        TCHAR	*Version;
        UINT	Length;
        VerQueryValue(Info, TEXT("\\StringFileInfo\\0c0904b0\\FileVersion"), (LPVOID *) &Version, &Length);

        _tcscpy_s(FileVersion, Version);

        free(Info);
    }
    else
    {
        _tcscpy_s(FileVersion, TEXT("Unknown"));
    }

    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    WORD OriginalAttribute = GetConsoleTextAttribute(hOut);

    SetConsoleTextAttribute(hOut, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY);
    _tprintf(Product);
    SetConsoleTextAttribute(hOut, OriginalAttribute);
    TCHAR Version[] = TEXT(" Version ");
    _tprintf(Version);
    SetConsoleTextAttribute(hOut, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY);
    _tprintf(FileVersion);
    SetConsoleTextAttribute(hOut, OriginalAttribute);
    TCHAR By[] = TEXT("\n By Adam Gates (adamgates84+software@gmail.com) - http://radsoft.boxathome.net/\n");
    _tprintf(By);
}
