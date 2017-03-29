// RadDir.cpp : Defines the entry point for the console application.
//

#include <windows.h>
#include <tchar.h>

#include <Rad/WinError.H>
//#include <Rad/Win/Reg.h>
#include <Rad/About/AboutMessage.h>
#include <Rad/ConsoleUtils.h>
#include <Rad/DirHelper.h>
#include <Rad/NumberFormat.h>

//#include "Environment.H"

static TCHAR	g_IniFileName[1024];

void InitIniFileName()
{
    HMODULE	Module = GetModuleHandle(NULL);
    GetModuleFileName(Module, g_IniFileName, 1024);
    TCHAR* s = g_IniFileName + _tcslen(g_IniFileName) - 4;
    _tcscpy_s(s, ARRAYSIZE(g_IniFileName) - (s - g_IniFileName), TEXT(".ini"));
}

void DisplayWelcomeMessage()
{
    HMODULE	Module = GetModuleHandle(NULL);
    DisplayAboutMessage(Module, TEXT("RadDir"));

    const HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    const WORD OriginalAttribute = GetConsoleTextAttribute(hOut);
    const WORD HiliteAttribute = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;

    _tprintf(TEXT("\nDisplays a list of files and subdirectories in a directory.\n\nRadDir <options> <directory>\n\nOptions:\n"));
    TCHAR* options[][2] = {
        { TEXT("?"), TEXT("Display Usage") },
        { TEXT("c"), TEXT("Use Thousand Separator") },
        { TEXT("w"), TEXT("Display Wide Format") },
        { TEXT("b"), TEXT("Display Bare Format") },
        { TEXT("h"), TEXT("Display Hidden Files") },
        { TEXT("s"), TEXT("Recurse into Subdriectories") },
        { TEXT("o"), TEXT("Sort order") },
        { TEXT("g"), TEXT("Use Human Readable Sizes") },
    };
    for (int i = 0; i < ARRAYSIZE(options); ++i)
    {
        SetConsoleTextAttribute(hOut, HiliteAttribute);
        _tprintf(_T("    /%s"), options[i][0]);
        SetConsoleTextAttribute(hOut, OriginalAttribute);
        _tprintf(_T("  %s\n"), options[i][1]);
    }
    _tprintf(TEXT(" use "));
    SetConsoleTextAttribute(hOut, HiliteAttribute);
    _tprintf(TEXT("-"));
    SetConsoleTextAttribute(hOut, OriginalAttribute);
    _tprintf(TEXT(" to negate an option\n"));
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

void DisplayTime(const FILETIME& time, bool ConvertToLocal)
{
    SYSTEMTIME stUTC, stLocal;
    if (FileTimeToSystemTime(&time, &stUTC) == 0)
        rad::ThrowWinError();
    if (ConvertToLocal)
    {
        if (SystemTimeToTzSpecificLocalTime(NULL, &stUTC, &stLocal) == 0)
            rad::ThrowWinError();
    }
    else
        stLocal = stUTC;


    TCHAR Date[1024];
    /*int Length =*/ GetDateFormat(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &stLocal, NULL, Date, ARRAYSIZE(Date));

    TCHAR Time[1024];
    /*int Length =*/ GetTimeFormat(LOCALE_USER_DEFAULT, TIME_NOSECONDS, &stLocal, NULL, Time, ARRAYSIZE(Time));

    _tprintf(_T("%10s %8s"), Date, Time);
}

void DisplaySize(ULARGE_INTEGER size, NUMBERFMT* nf, bool human)
{
    TCHAR m[] = TEXT(" KMG");
    int i = 0;
    UINT NumDigits = nf->NumDigits;

    TCHAR NumberIn[1024];

    if (human)
    {
        double s = (double) size.QuadPart;
        while (s > (1024 * 9 / 10) && i < ARRAYSIZE(m))
        {
            s /= 1024;
            ++i;
        }
        if (i == 0)
        {
            nf->NumDigits = 0;
        }

        _stprintf_s(NumberIn, TEXT("%f"), s);
    }
    else
    {
        nf->NumDigits = 0;
        _stprintf_s(NumberIn, TEXT("%d"), (int) size.QuadPart);
    }

    TCHAR Number[1024];
    /*int Length =*/ GetNumberFormat(LOCALE_USER_DEFAULT, 0, NumberIn, nf, Number, ARRAYSIZE(Number));

    if (human)
        _tprintf(_T("%13s%c"), Number, m[i]);
    else
        _tprintf(_T("%14s"), Number);
    nf->NumDigits = NumDigits;
}

const TCHAR* GetExtension(const TCHAR *Name)
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

void DisplayName(const CDirectory::CEntry& dir_entry)
{
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    WORD OriginalAttribute = GetConsoleTextAttribute(hOut);
    WORD Attribute = OriginalAttribute;
    {
        DWORD Value = 0;
        const TCHAR *KeyName = 0;
        if (dir_entry.IsDirectory())
        {
            KeyName = TEXT("Directory");
        }
        else
        {
            KeyName = GetExtension(dir_entry.GetFileName());
        }

#if 0
        HKEY Key;
        DWORD Type = 0;
        DWORD Length = sizeof(Value);
        LONG Error;
        Error = RegOpenKey(HKEY_CLASSES_ROOT, KeyName, &Key);
        Error = RegQueryValueEx(Key, TEXT("Colour"), 0, &Type, (LPBYTE) &Value, &Length);
        RegCloseKey(Key);
#else
        Value = GetPrivateProfileInt(TEXT("Colour"), KeyName, Value, g_IniFileName);
#endif

        if (Value > 0)
        {
            if (Value <= 0xFF)
                Attribute &= 0xFF00;
            else
                Attribute = 0;
            Attribute |= Value;
        }
    }

    SetConsoleTextAttribute(hOut, Attribute);

    _tprintf(dir_entry.GetFileName());

    SetConsoleTextAttribute(hOut, OriginalAttribute);
}

void DisplayFileDataLong(const CDirectory::CEntry& dir_entry, NUMBERFMT* nf, bool human, bool ConvertToLocal)
{
    _tprintf(TEXT("%c%c%c%c%c ")
        , (dir_entry.IsDirectory() ? TEXT('D') : TEXT('-'))
        , (dir_entry.IsReadOnly() ? TEXT('R') : TEXT('-'))
        , (dir_entry.IsArchive() ? TEXT('A') : TEXT('-'))
        , (dir_entry.IsSystem() ? TEXT('S') : TEXT('-'))
        , (dir_entry.IsHidden() ? TEXT('H') : TEXT('-'))
        );
    DisplayTime(dir_entry.ftLastWriteTime, ConvertToLocal);
    if (dir_entry.IsDirectory())
        _tprintf(_T("%14s"), _T(""));
    else
        DisplaySize(dir_entry.GetFileSize(), nf, human);
    _tprintf(TEXT(" "));
    DisplayName(dir_entry);
    _tprintf(TEXT("\n"));
}

void DisplayFileDataWide(const CDirectory::CEntry& dir_entry)
{
    if (dir_entry.IsDirectory())
        _tprintf(TEXT("["));
    DisplayName(dir_entry);
    if (dir_entry.IsHidden())
        _tprintf(TEXT("^"));
    if (dir_entry.IsReparsePoint())
        _tprintf(TEXT("@"));
    if (dir_entry.IsDirectory())
        _tprintf(TEXT("]"));
}

void DisplayFileDataBare(const CDirectory::CEntry& dir_entry)
{
    DisplayName(dir_entry);
    _tprintf(TEXT("\n"));
}

void DisplayDirListSummary(const std::vector<CDirectory::CEntry>& dirlist, NUMBERFMT* nf, bool human)
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
    _tprintf(TEXT("    %d File(s)   %d Dir(s) "), CountFiles, CountDirectories);
    DisplaySize(CountSize, nf, human);
    _tprintf(TEXT(" bytes\n"));
}

void DisplayDirListLong(const std::vector<CDirectory::CEntry>& dirlist, NUMBERFMT* nf, bool human, bool ConvertToLocal)
{
    for (std::vector<CDirectory::CEntry>::const_iterator it = dirlist.begin();
        it != dirlist.end(); ++it)
    {
        if (!it->IsDots())
            DisplayFileDataLong(*it, nf, human, ConvertToLocal);
    }
    DisplayDirListSummary(dirlist, nf, human);
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

void DisplayDirListWide(const std::vector<CDirectory::CEntry>& dirlist, NUMBERFMT* nf, bool human)
{
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    int ScreenWidth = GetConsoleWidth(hOut);
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
            DisplayFileDataWide(*it);
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
    DisplayDirListSummary(dirlist, nf, human);
}

void DisplayDirListBare(const std::vector<CDirectory::CEntry>& dirlist)
{
    for (std::vector<CDirectory::CEntry>::const_iterator it = dirlist.begin();
        it != dirlist.end(); ++it)
    {
        if (!it->IsDots())
            DisplayFileDataBare(*it);
    }
}

struct Config
{
    bool UseThousandSep;
    bool GroupDirectoriesFirst;
    DirSorter::Order order;
    bool DisplayWideFormat;
    bool DisplayBareFormat;
    bool DisplayHiddenFiles;
    bool Recursive;
    bool Human;
};

void DoDirectory(const Url& DirPattern, const Config& config, NumberFormat& nf)
{
    std::vector<CDirectory::CEntry> dirlist;
    dirlist.reserve(1000);

    try
    {
        GetDirectory(DirPattern, dirlist, config.DisplayHiddenFiles);
    }
    catch (const rad::WinError&)
    {
        if (!config.Recursive)
            throw;
    }

    if (config.order != DirSorter::None)
        SortDirectory(dirlist, DirSorter(config.order, config.GroupDirectoriesFirst));

    if (!config.Recursive || !dirlist.empty())
    {
        if (config.Recursive)
            _tprintf(TEXT("Directory: %s\n"), DirPattern.GetUrl());

        if (config.DisplayBareFormat)
            DisplayDirListBare(dirlist);
        else if (config.DisplayWideFormat)
            DisplayDirListWide(dirlist, &nf, config.Human);
        else
            DisplayDirListLong(dirlist, &nf, config.Human, DirPattern.IsLocal());
        //_tprintf(TEXT("\n"));
    }

    if (config.Recursive)
    {
        const TCHAR* last = _tcsrchr(DirPattern.GetUrl(), DirPattern.GetDelim());
        std::tstring Pattern(DirPattern.GetUrl());
        if (last)
            Pattern.assign(last + 1);
        // This is is really try to decide if we already have the subdirectories
        if (Pattern.empty() || Pattern == TEXT("*") || (Pattern.find(TEXT('*')) == std::tstring::npos && Pattern.find(TEXT('?')) == std::tstring::npos))
        {
            for (std::vector<CDirectory::CEntry>::const_iterator it = dirlist.begin();
                it != dirlist.end(); ++it)
            {
                if (it->IsDirectory() && !it->IsDots())
                {
                    Url SubDirPattern(Pattern == TEXT("*") ? it->GetFileName() : DirPattern + it->GetFileName());
                    SubDirPattern.AppendDelim();
                    DoDirectory(SubDirPattern, config, nf);
                }
            }
        }
        else
        {
            // TODO Get the direcory again without the pattern
            std::tstring BaseSubDir;
            if (last)
                BaseSubDir.assign(DirPattern.GetUrl(), last + 1);
            GetDirectory(BaseSubDir.c_str(), dirlist, config.DisplayHiddenFiles);
            for (std::vector<CDirectory::CEntry>::const_iterator it = dirlist.begin();
                it != dirlist.end(); ++it)
            {
                if (it->IsDirectory() && !it->IsDots())
                {
                    Url SubDirPattern(BaseSubDir + it->GetFileName());
                    SubDirPattern.Change(Pattern);
                    DoDirectory(SubDirPattern, config, nf);
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
        config.UseThousandSep = true;
        config.GroupDirectoriesFirst = true;
        config.order = DirSorter::Name;
        config.DisplayWideFormat = true;
        config.DisplayBareFormat = false;
        config.DisplayHiddenFiles = false;
        config.Recursive = false;
        config.Human = true;

        BoolArg boolArg[] =
        {
            { TEXT('?'), &DisplayWelcomeMsg },
            { TEXT('c'), &config.UseThousandSep },
            { TEXT('C'), &config.UseThousandSep },
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
                        config.order = DirSorter::Name;
                        break;

                    case TEXT('S'):
                    case TEXT('s'):
                        config.order = DirSorter::Size;
                        break;

                    case TEXT('D'):
                    case TEXT('d'):
                        config.order = DirSorter::Date;
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

        NumberFormat	nf(LOCALE_USER_DEFAULT);
        //nf.NumDigits = 0;
        if (!config.UseThousandSep)
            nf.Grouping = 0;

        if (DisplayWelcomeMsg)
            DisplayWelcomeMessage();
        else
        {
            InitIniFileName();
            Url d(DirPattern);
            if (_tcschr(DirPattern, '*') == NULL && CDirectory::Exists(DirPattern)) // TODO Should this go in GetDirectory??
                d.AppendDelim();
            DoDirectory(d, config, nf);
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
