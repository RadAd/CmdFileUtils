// RadDir.cpp : Defines the entry point for the console application.
//

#include <windows.h>
#include <tchar.h>
#include <Shlwapi.h>
#include <fileapi.h>
#include <io.h>

#include <Rad/WinError.H>
//#include <Rad/Win/Reg.h>
#include <Rad/About/AboutMessage.h>
#include <Rad/DirHelper.h>

//#include "Environment.H"

#define ESC TEXT("\x1B")
#define ANSI_COLOR ESC TEXT("[%sm")
#define ANSI_COLOR_(x) ESC TEXT("[") TEXT(#x) TEXT("m")
#define ANSI_RESET ESC TEXT("[0m")

BOOL GetRealPath(LPCWSTR File, LPWSTR Path, int Len)
{
    rad::WinHandle<> hFile = CreateFile(File,               // file to open
        GENERIC_READ,          // open for reading
        FILE_SHARE_READ,       // share for reading
        NULL,                  // default security
        OPEN_EXISTING,         // existing file only
        FILE_FLAG_BACKUP_SEMANTICS | FILE_ATTRIBUTE_NORMAL, // normal file
        NULL);                 // no attr. template

    if (!hFile)
        return FALSE;

    DWORD dwRet = GetFinalPathNameByHandle(hFile.Get(), Path, Len, VOLUME_NAME_DOS);

    if (dwRet == 0)
        return EXIT_FAILURE;

    if (_tcsnccmp(Path, _T("\\\\?\\"), 4) == 0)
        _tcscpy_s(Path, Len, Path + 4);

    return TRUE;
}

static TCHAR	g_IniFileName[1024];

void InitIniFileName()
{
    HMODULE	Module = GetModuleHandle(NULL);
    GetModuleFileName(Module, g_IniFileName, 1024);
    TCHAR* s = g_IniFileName + _tcslen(g_IniFileName) - 4;
    _tcscpy_s(s, ARRAYSIZE(g_IniFileName) - (s - g_IniFileName), TEXT(".ini"));
}

void DisplayWelcomeMessage(bool Ansi)
{
    HMODULE	Module = GetModuleHandle(NULL);
    DisplayAboutMessage(Module, TEXT("RadDir"));

    _tprintf(TEXT("\nDisplays a list of files and subdirectories in a directory.\n\nRadDir <options> <directory>\n\nOptions:\n"));
    TCHAR* options[][2] = {
        { TEXT("?"), TEXT("Display Usage") },
        { TEXT("w"), TEXT("Display Wide Format") },
        { TEXT("b"), TEXT("Display Bare Format") },
        { TEXT("h"), TEXT("Display Hidden Files") },
        { TEXT("s"), TEXT("Recurse into Subdriectories") },
        { TEXT("o"), TEXT("Sort order") },
        { TEXT("g"), TEXT("Use Human Readable Sizes") },
    };
    if (Ansi)
    {
        for (int i = 0; i < ARRAYSIZE(options); ++i)
            _tprintf(_T("    ") ANSI_COLOR_(37) _T("/%s") ANSI_RESET _T("  %s\n"), options[i][0], options[i][1]);
        _tprintf(TEXT(" use ") ANSI_COLOR_(37) _T("-") ANSI_RESET _T(" to negate an option\n"));
    }
    else
    {
        for (int i = 0; i < ARRAYSIZE(options); ++i)
            _tprintf(_T("    /%s  %s\n"), options[i][0], options[i][1]);
        _tprintf(TEXT(" use - to negate an option\n"));
    }
}

void DisplayPadding(int Count)
{
    while (Count > 4)
    {
        _tprintf(TEXT("    "));
        Count -= 4;
    }
    while(Count > 0)
    {
        _tprintf(TEXT(" "));
        --Count;
    }
}

void DisplayTime(const FILETIME& time)
{
    // TODO Need to test if this is showing ftp filetime correctly - may need to convert to localtime
    TCHAR Date[1024];
    DWORD dwDateFlags = FDTF_SHORTDATE | FDTF_NOAUTOREADINGORDER;
    SHFormatDateTime(&time, &dwDateFlags, Date, ARRAYSIZE(Date));
    TCHAR Time[1024];
    DWORD dwTimeFlags = FDTF_SHORTTIME | FDTF_NOAUTOREADINGORDER;
    SHFormatDateTime(&time, &dwTimeFlags, Time, ARRAYSIZE(Time));
    _tprintf(_T("%10s %8s"), Date, Time);
}

void DisplaySize(ULARGE_INTEGER size, bool human)
{
    TCHAR Number[1024];
    if (human)
    {
        // TODO Try StrFormatByteSizeEx
        // Note: In Windows 10, size is reported in base 10 rather than base 2. For example, 1 KB is 1000 bytes rather than 1024.
        StrFormatByteSize64(size.QuadPart, Number, ARRAYSIZE(Number));
        const size_t len = _tcslen(Number);
        if (_tcscmp(Number + len - 5, _T("bytes")) == 0)
            _tcscpy_s(Number + len - 5, ARRAYSIZE(Number) - len + 5, _T(" B"));
    }
    else
    {
        _stprintf_s(Number, TEXT("%llu"), size.QuadPart);
    }
    _tprintf(_T("%14s"), Number);
}

const TCHAR* GetExtension(const TCHAR *Name) // TODO Replace with PathFindExtension
{
    const TCHAR* Ext = Name + _tcslen(Name);
    while(Ext > Name)
    {
        if (*Ext == CDirectory::GetDelim())
        {
            return TEXT("");
        }
        else if (*Ext == TEXT('.'))
        {
            return Ext;
        }
        --Ext;
    };
    return TEXT("");
}

void DisplayName(const std::tstring& BaseDir, const CDirectory::CEntry& dir_entry, bool Ansi)
{
    TCHAR strValue[1024] = TEXT("");
    if (Ansi)
    {
        const TCHAR* KeyName = dir_entry.IsDirectory() ? TEXT("Directory") : GetExtension(dir_entry.GetFileName());
        GetPrivateProfileString(TEXT("Colour"), KeyName, TEXT(""), strValue, ARRAYSIZE(strValue), g_IniFileName);
    }

    if (Ansi && strValue[0] != TEXT('\0'))
        _tprintf(ANSI_COLOR, strValue);

    if (Ansi && !BaseDir.empty())
        _tprintf(ESC TEXT("]8;;file://%s%s") ESC TEXT("\\"), BaseDir.c_str(), dir_entry.GetFileName());

    _tprintf(dir_entry.GetFileName());

    if (Ansi && !BaseDir.empty())
        _tprintf(ESC TEXT("]8;;") ESC TEXT("\\"));

    if (Ansi && strValue[0] != TEXT('\0'))
        _tprintf(ANSI_RESET);
}

void DisplayFileDataLong(const std::tstring& BaseDir, const CDirectory::CEntry& dir_entry, bool human, bool Ansi)
{
    _tprintf(TEXT("%c%c%c%c%c ")
        , (dir_entry.IsDirectory() ? TEXT('D') : TEXT('-'))
        , (dir_entry.IsReadOnly() ? TEXT('R') : TEXT('-'))
        , (dir_entry.IsArchive() ? TEXT('A') : TEXT('-'))
        , (dir_entry.IsSystem() ? TEXT('S') : TEXT('-'))
        , (dir_entry.IsHidden() ? TEXT('H') : TEXT('-'))
        );
    DisplayTime(dir_entry.ftLastWriteTime);
    if (dir_entry.IsDirectory())
        _tprintf(_T("%14s"), _T(""));
    else
        DisplaySize(dir_entry.GetFileSize(), human);
    _tprintf(TEXT(" "));
    DisplayName(BaseDir, dir_entry, Ansi);
    if (dir_entry.IsReparsePoint())
    {
        Url FullPath(BaseDir + dir_entry.GetFileName());
        TCHAR RealPath[MAX_PATH];
        GetRealPath(FullPath.GetUrl(), RealPath, ARRAYSIZE(RealPath));
        _tprintf(TEXT(" [%s]"), RealPath);
    }
    _tprintf(TEXT("\n"));
}

void DisplayFileDataWide(const std::tstring& BaseDir, const CDirectory::CEntry& dir_entry, bool Ansi)
{
    if (dir_entry.IsDirectory())
        _tprintf(TEXT("["));
        //printf(u8"\U0001f4c1");
    DisplayName(BaseDir, dir_entry, Ansi);
    if (dir_entry.IsHidden())
        _tprintf(TEXT("^"));
    if (dir_entry.IsReparsePoint())
        _tprintf(TEXT("@"));
    if (dir_entry.IsDirectory())
        _tprintf(TEXT("]"));
}

void DisplayFileDataBare(const std::tstring& BaseDir, const CDirectory::CEntry& dir_entry)
{
    if (!BaseDir.empty())
        _tprintf(BaseDir.c_str());
    DisplayName(TEXT(""), dir_entry, false);
    _tprintf(TEXT("\n"));
}

void DisplayDirListSummary(const std::vector<CDirectory::CEntry>& dirlist, bool human)
{
    int CountFiles = 0;
    int CountDirectories = 0;
    ULARGE_INTEGER CountSize = {};
    for (std::vector<CDirectory::CEntry>::const_iterator it = dirlist.begin();
        it != dirlist.end(); ++it)
    {
        if (it->IsDirectory())
            ++CountDirectories;
        else
            ++CountFiles;
        CountSize.QuadPart += it->GetFileSize().QuadPart;
    }
    _tprintf(TEXT("  %3d File(s) %3d Dir(s) "), CountFiles, CountDirectories);
    DisplaySize(CountSize, human);
    _tprintf(TEXT("\n"));
}

void DisplayDirListLong(const std::tstring& BaseDir, const std::vector<CDirectory::CEntry>& dirlist, bool human, bool Ansi)
{
    for (std::vector<CDirectory::CEntry>::const_iterator it = dirlist.begin();
        it != dirlist.end(); ++it)
    {
        if (!it->IsDots())
            DisplayFileDataLong(BaseDir, *it, human, Ansi);
    }
    DisplayDirListSummary(dirlist, human);
}

int GetFileNameLenWide(const CDirectory::CEntry& dir_entry)
{
    int Length = (int) dir_entry.GetFileNameLen();
    if (dir_entry.IsDirectory())
        Length += 2;
    if (dir_entry.IsHidden())
        Length += 1;
    if (dir_entry.IsReparsePoint())
        Length += 1;
    return Length;
}

int GetConsoleWidth()
{
    _cputts(ESC TEXT("7"));            // Save cursor position
    _cputts(ESC TEXT("[999;999H"));    // Move cursor
    _cputts(ESC TEXT("[6n"));          // Get cursor position

    TCHAR buf[100] = TEXT("");
    TCHAR* w = nullptr;
    {
        TCHAR* p = buf;
        do
        {
            *p = _gettch();
            if (*p == TEXT(';'))
                w = p;
            ++p;
        } while (p[-1] != TEXT('R'));
        *p = TEXT('\0');
    }

    _cputts(ESC TEXT("8"));            // Restore cursor position
    return w == nullptr ? 100 : _tstoi(w + 1);
}

void DisplayDirListWide(const std::tstring& BaseDir, const std::vector<CDirectory::CEntry>& dirlist, bool human, bool Ansi)
{
    const int ScreenWidth = GetConsoleWidth();
    int Width = 12;
    for (std::vector<CDirectory::CEntry>::const_iterator it = dirlist.begin();
        it != dirlist.end(); ++it)
    {
        if (!it->IsDots())
        {
            int Length = GetFileNameLenWide(*it);
            if (Length > Width)
                Width = Length;
        }
    }
    ++Width;
    int NoColumns = ScreenWidth / Width;
    int Column = 0;
    for (std::vector<CDirectory::CEntry>::const_iterator it = dirlist.begin();
        it != dirlist.end(); ++it)
    {
        if (!it->IsDots())
        {
            DisplayFileDataWide(BaseDir, *it, Ansi);
            ++Column;
            if (Column >= NoColumns)
            {
                Column = 0;
                _tprintf(TEXT("\n"));
            }
            else
                DisplayPadding(Width - GetFileNameLenWide(*it));
        }
    }
    if (Column != 0)
        _tprintf(TEXT("\n"));
    DisplayDirListSummary(dirlist, human);
}

void DisplayDirListBare(const std::tstring& BaseDir, const std::vector<CDirectory::CEntry>& dirlist)
{
    for (std::vector<CDirectory::CEntry>::const_iterator it = dirlist.begin();
        it != dirlist.end(); ++it)
    {
        if (!it->IsDots())
            DisplayFileDataBare(BaseDir, *it);
    }
}

struct Config
{
    bool GroupDirectoriesFirst;
    DirSorter::Order order;
    bool DisplayWideFormat;
    bool DisplayBareFormat;
    bool DisplayHiddenFiles;
    bool Recursive;
    bool Human;
    bool Ansi;
};

std::tstring GetFullPathName(const TCHAR* lpFileName, std::tstring& Pattern)
{
    TCHAR path[MAX_PATH];
    TCHAR* lpFilePart = nullptr;
    GetFullPathName(
        lpFileName[0] != TEXT('\0') ? lpFileName : TEXT(".\\"),
        ARRAYSIZE(path),
        path,
        &lpFilePart
    );
    if (lpFilePart != nullptr)
    {
        Pattern = lpFilePart;
        *lpFilePart = TEXT('\0');
    }
    else
        Pattern.clear();
    return path;
}

std::tstring GetPath(LPCTSTR filepath)
{
    TCHAR path[1024];
    _tcscpy_s(path, filepath);
    PathRemoveFileSpec(path);
    return path;
}

void DoDirectory(const Url& DirPattern, const Config& config)
{
    std::vector<CDirectory::CEntry> dirlist;
    dirlist.reserve(1000);

    GetDirectory(DirPattern, dirlist, config.DisplayHiddenFiles);

    if (config.order != DirSorter::Order::None)
        SortDirectory(dirlist, DirSorter(config.order, config.GroupDirectoriesFirst));

    if (!config.Recursive || !dirlist.empty())
    {
        if (config.Recursive && !config.DisplayBareFormat)
            _tprintf(TEXT("Directory: %s\n"), DirPattern.GetUrl());

        std::tstring FullPattern;
        const std::tstring FullBaseDir = GetFullPathName(DirPattern.GetPath(), FullPattern);

        if (config.DisplayBareFormat)
            DisplayDirListBare(config.Recursive ? GetPath(DirPattern.GetPath()) : TEXT(""), dirlist);
        else if (config.DisplayWideFormat)
            DisplayDirListWide(FullBaseDir, dirlist, config.Human, config.Ansi);
        else
            DisplayDirListLong(FullBaseDir, dirlist, config.Human, config.Ansi);
        //_tprintf(TEXT("\n"));
    }

    if (config.Recursive)
    {
        const TCHAR* last = _tcsrchr(DirPattern.GetUrl(), DirPattern.GetDelim());

        const std::tstring Pattern(last ? last + 1 : DirPattern.GetUrl());

        // This is is really try to decide if we already have the subdirectories
        if (Pattern.empty() || Pattern == TEXT("*") || (Pattern.find(TEXT('*')) == std::tstring::npos && Pattern.find(TEXT('?')) == std::tstring::npos))
        {
            for (std::vector<CDirectory::CEntry>::const_iterator it = dirlist.begin();
                it != dirlist.end(); ++it)
            {
                if (it->IsDirectory() && !it->IsDots() && (config.DisplayHiddenFiles || !it->IsHidden()))
                {
                    Url SubDirPattern(Pattern == TEXT("*") ? it->GetFileName() : DirPattern + it->GetFileName());
                    SubDirPattern.AppendDelim();
                    DoDirectory(SubDirPattern, config);
                }
            }
        }
        else
        {
            const std::tstring BaseDir = last ? std::tstring(DirPattern.GetUrl(), last + 1) : std::tstring();

            // TODO Get the directory again without the pattern
            GetDirectory(BaseDir.c_str(), dirlist, config.DisplayHiddenFiles);
            for (std::vector<CDirectory::CEntry>::const_iterator it = dirlist.begin();
                it != dirlist.end(); ++it)
            {
                if (it->IsDirectory() && !it->IsDots())
                {
                    Url SubDirPattern(BaseDir + it->GetFileName());
                    SubDirPattern.Change(Pattern);
                    DoDirectory(SubDirPattern, config);
                }
            }
        }
    }
}

struct BoolArg
{
    TCHAR  m_Char;
    bool* m_Bool;
};

int tmain(int argc, TCHAR* argv[])
{
    try
    {
        bool DisplayWelcomeMsg = false;
        const TCHAR* DirPattern = TEXT("");
        Config config;
        config.GroupDirectoriesFirst = true;
        config.order = DirSorter::Order::Name;
        config.DisplayWideFormat = true;
        config.DisplayBareFormat = false;
        config.DisplayHiddenFiles = false;
        config.Recursive = false;
        config.Human = true;
        config.Ansi = _isatty(_fileno(stdout));

        BoolArg boolArg[] =
        {
            { TEXT('?'), &DisplayWelcomeMsg },
            { TEXT('w'), &config.DisplayWideFormat },
            { TEXT('W'), &config.DisplayWideFormat },
            { TEXT('b'), &config.DisplayBareFormat },
            { TEXT('B'), &config.DisplayBareFormat },
            { TEXT('h'), &config.DisplayHiddenFiles },
            { TEXT('H'), &config.DisplayHiddenFiles },
            { TEXT('s'), &config.Recursive },
            { TEXT('S'), &config.Recursive },
            { TEXT('g'), &config.Human },
            { TEXT('G'), &config.Human },
            { TEXT('\0'), 0 }
        };

        // TODO Get defaults from DIRCMD

        int arg = 1;
        while (arg < argc)
        {
            if (argv[arg][0] == TEXT('/'))
            {
                if (argv[arg][1] == TEXT('O' || argv[arg][1] == 'o'))
                {
                    config.GroupDirectoriesFirst = false;
                    int the_char = 2;
                    if (argv[arg][the_char] == TEXT(':'))
                        ++the_char;
                    if (argv[arg][the_char] == TEXT('G' || argv[arg][the_char] == 'g'))
                    {
                        config.GroupDirectoriesFirst = true;
                        ++the_char;
                    }
                    switch (argv[arg][the_char])
                    {
                    case TEXT('N'):
                    case TEXT('n'):
                        config.order = DirSorter::Order::Name;
                        break;

                    case TEXT('S'):
                    case TEXT('s'):
                        config.order = DirSorter::Order::Size;
                        break;

                    case TEXT('D'):
                    case TEXT('d'):
                        config.order = DirSorter::Order::Date;
                        break;
                    }
                }
                else
                {
                    int Pos = 1;
                    bool Value = true;
                    if (argv[arg][1] == TEXT('-'))
                    {
                        Pos = 2;
                        Value = false;
                    }

                    BoolArg* thisBoolArg = boolArg;
                    while (thisBoolArg->m_Char != argv[arg][Pos] && thisBoolArg->m_Char != TEXT('\0'))
                        ++thisBoolArg;
                    if (thisBoolArg->m_Bool)
                        *(thisBoolArg->m_Bool) = Value;
                }
            }
            else
                DirPattern = argv[arg];
            ++arg;
        }

        if (DisplayWelcomeMsg)
            DisplayWelcomeMessage(config.Ansi);
        else
        {
            InitIniFileName();
            Url d(DirPattern);
            if (d.nScheme == INTERNET_SCHEME_DEFAULT && _tcschr(d.GetPath(), _T('*')) == NULL && CDirectory::Exists(d.GetPath())) // TODO Should this go in GetDirectory??
                d.AppendDelim();
            DoDirectory(d, config);
        }
    }
    catch(const rad::WinError& e)
    {
        _ftprintf(stderr, e.GetString().c_str());
        return EXIT_FAILURE + 0;
    }
    catch(...)
    {
        _ftprintf(stderr, TEXT("Unknown Exception\n"));
        return EXIT_FAILURE + 1;
    }

    return EXIT_SUCCESS;
}
