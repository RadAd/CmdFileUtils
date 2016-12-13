#include <assert.h>

#include <Rad/WinError.H>
#include <Rad/Win/WinFile.H>
#include <Rad/About/AboutMessage.h>
#include <Rad/ConsoleUtils.h>
#include <Rad/DirHelper.h>
#include <Rad/NumberFormat.h>

void DisplayWelcomeMessage()
{
    const HMODULE	Module = GetModuleHandle(NULL);
    DisplayAboutMessage(Module, TEXT("RadSync"));

    const HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    const WORD OriginalAttribute = GetConsoleTextAttribute(hOut);
    const WORD HiliteAttribute = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;

    _tprintf(_T("\nOne way sync between two directories.\n\nRadSync <options> [src] [dst]\n\nOptions:\n"));
    TCHAR* options[][2] = {
        { TEXT("t"),  TEXT("Test - do not change anything") },
        { TEXT("h"),  TEXT("Hidden - copy hidden files as well") },
        // { TEXT("d"),  TEXT("Delete - delete files in destination") }, TODO
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

struct Options
{
    Options()
        : nf(LOCALE_USER_DEFAULT)
        , test(false)
        , hidden(false)
    {
    }

    NumberFormat nf;
    bool test;
    bool hidden;
};

void SyncDirectories(Url dirA, Url dirB, const Options& options);

void SyncCopyFileDir(const Url& source, const CDirectory::CEntry& source_entry, const Url& destination, const Options& options)
{
    if (source_entry.IsDirectory())
    {
        //_tprintf(_T("Copy directory %s from %s to %s\n"), source_entry.GetFileName(), source.GetUrl(), destination.GetUrl());
        if (!options.test)
        {
            Url subdirdest = destination + source_entry.GetFileName();
            subdirdest.CreateDirectory();
            SyncDirectories(source + source_entry.GetFileName(), subdirdest, options);
        }
    }
    else
    {
        //_tprintf(_T("Copy file %s from %s to %s\n"), source_entry.GetFileName(), source.GetUrl(), destination.GetUrl());
        if (!options.test)
        {
            CopyFile(source, destination, source_entry, &options.nf);
        }
    }
}

void SyncDirectories(const Url& dirA, const std::vector<CDirectory::CEntry>& dirlistA, const Url& dirB, const std::vector<CDirectory::CEntry>& dirlistB, const Options& options)
{
    std::vector<CDirectory::CEntry>::const_iterator dirlistA_it = dirlistA.begin();
    std::vector<CDirectory::CEntry>::const_iterator dirlistB_it = dirlistB.begin();

    while (dirlistA_it != dirlistA.end() || dirlistB_it != dirlistB.end())
    {
        if (dirlistA_it != dirlistA.end() && _tcscmp(dirlistA_it->GetFileName(), TEXT(".")) == 0)
            ++dirlistA_it;
        else if (dirlistA_it != dirlistA.end() && _tcscmp(dirlistA_it->GetFileName(), TEXT("..")) == 0)
            ++dirlistA_it;
        else if (dirlistB_it != dirlistB.end() && _tcscmp(dirlistB_it->GetFileName(), TEXT(".")) == 0)
            ++dirlistB_it;
        else if (dirlistB_it != dirlistB.end() && _tcscmp(dirlistB_it->GetFileName(), TEXT("..")) == 0)
            ++dirlistB_it;
        else
        {
            int compare;
            if (dirlistA_it == dirlistA.end())
                compare = 1;
            else if (dirlistB_it == dirlistB.end())
                compare = -1;
            else
                compare = _tcsicmp(dirlistA_it->GetFileName(), dirlistB_it->GetFileName());
            if (compare == 0)
            {   // Same file name
                if (!dirlistA_it->IsDirectory() && !dirlistB_it->IsDirectory())
                {
                    // TODO Hidden files? Sync anyway?
                    if (dirlistA_it->nFileSizeHigh != dirlistB_it->nFileSizeHigh
                        || dirlistA_it->nFileSizeLow != dirlistB_it->nFileSizeLow
                        || dirlistA_it->ftLastWriteTime != dirlistB_it->ftLastWriteTime)
                    {
                        // TODO Think about sync direction
#if 0
                        if (dirlistA_it->ftLastWriteTime == dirlistB_it->ftLastWriteTime)
                        {
                            _tprintf(_T(" ! %s\n"), dirlistA_it->GetFileName());
                            _tprintf(_T("Warning: File %s different size but same time.\n"), dirlistA_it->GetFileName());
                        }
                        else if (dirlistA_it->ftLastWriteTime > dirlistB_it->ftLastWriteTime)
                        {
                            _tprintf(_T(" > %s\n"), dirlistA_it->GetFileName());
                            if (!options.test)
                            {
                                CopyFile(dirA, dirB, *dirlistA_it, &options.nf);
                            }
                        }
                        else
                        {
                            _tprintf(_T(" < %s\n"), dirlistB_it->GetFileName());
                            if (!options.test)
                            {
                                CopyFile(dirB, dirA, *dirlistB_it, &options.nf);
                            }
                        }
#else
                        if (dirlistA_it->ftLastWriteTime > dirlistB_it->ftLastWriteTime)
                        {
                            _tprintf(_T(" > %s\n"), dirlistA_it->GetFileName());
                            if (!options.test)
                            {
                                CopyFile(dirA, dirB, *dirlistA_it, &options.nf);
                            }
                        }
                        else if (dirlistA_it->nFileSizeHigh != dirlistB_it->nFileSizeHigh
                            || dirlistA_it->nFileSizeLow != dirlistB_it->nFileSizeLow)
                        {
                            _tprintf(_T(" ! %s\n"), dirlistA_it->GetFileName());
                            _tprintf(_T("Warning: File %s different size but destination has later time.\n"), dirlistA_it->GetFileName());
                        }
                        //else
                        //_tprintf(_T(" = %s\n"), dirlistA_it->GetFileName());
#endif
                    }
                    //else
                    //_tprintf(_T(" = %s\n"), dirlistA_it->GetFileName());
                }
                else if (dirlistA_it->IsDirectory() && dirlistB_it->IsDirectory())
                {
                    SyncDirectories(dirA + dirlistA_it->GetFileName(), dirB + dirlistB_it->GetFileName(), options);
                }
                else if (dirlistA_it->IsDirectory() != dirlistB_it->IsDirectory())
                {
                    _tprintf(_T("Warning: File %s shares name with a directory.\n"), dirlistA_it->GetFileName());
                }
                ++dirlistA_it;
                ++dirlistB_it;
            }
            else if (compare < 0)
            {
                if (options.hidden || !dirlistA_it->IsHidden())
                {
                    _tprintf(_T(" > %s\n"), dirlistA_it->GetFileName());
                    SyncCopyFileDir(dirA, *dirlistA_it, dirB, options);
                }
                ++dirlistA_it;
            }
            else //if (compare > 0)
            {
#if 0
                if (hidden || !dirlistB_it->IsHidden())
                {
                    _tprintf(_T(" < %s\n"), dirlistB_it->GetFileName());
                    SyncCopyFileDir(dirB, *dirlistB_it, dirA, test, hidden);
                }
#else
                // TODO Delete desination file
                _tprintf(_T(" X %s\n"), dirlistB_it->GetFileName());
#endif
                ++dirlistB_it;
            }
        }
    }
}

void SyncDirectories(Url dirA, Url dirB, const Options& options)
{
    // TODO Directories as sorted first so need to output this at the appropriate time
    _tprintf(_T("# %s\n"), dirB.GetPath());

    std::vector<CDirectory::CEntry> dirlistA;
    std::vector<CDirectory::CEntry> dirlistB;

    dirA.AppendDelim();
    dirB.AppendDelim();

    GetDirectory(dirA, dirlistA, true);
    GetDirectory(dirB, dirlistB, true);

    DirSorter dir_sorter(DirSorter::Name, true);
    SortDirectory(dirlistA, dir_sorter);
    SortDirectory(dirlistB, dir_sorter);

    SyncDirectories(dirA, dirlistA, dirB, dirlistB, options);
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
        Options options;
        std::vector<const TCHAR*> my_args;

        BoolArg boolArg[] =
        {
            { TEXT('t'), &options.test },
            { TEXT('h'), &options.hidden },
            { TEXT('\0'), 0 }
        };

        int arg = 1;
        while (arg < argc)
        {
            if (argv[arg][0] == TEXT('/'))
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
            else
                my_args.push_back(argv[arg]);
            ++arg;
        }

        if (my_args.size() == 2)
        {
            SyncDirectories(my_args[0], my_args[1], options);
        }
        else
            DisplayWelcomeMessage();
    }
    catch (const rad::WinError& e)
    {
        _ftprintf(stderr, e.GetString().c_str());
        return EXIT_FAILURE + 0;
    }
    catch (...)
    {
        _ftprintf(stderr, TEXT("Unknown Exception\n"));
        return EXIT_FAILURE + 1;
    }

    return EXIT_SUCCESS;
}
