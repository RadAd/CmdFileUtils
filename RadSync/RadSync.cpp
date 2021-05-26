#include <assert.h>
#include <io.h>

#include <Rad/WinError.H>
#include <Rad/Win/WinFile.H>
#include <Rad/About/AboutMessage.h>
#include <Rad/DirHelper.h>
#include <Rad/NumberFormat.h>

#define ESC TEXT("\x1B")
#define ANSI_COLOR ESC TEXT("[%sm")
#define ANSI_COLOR_(x) ESC TEXT("[") TEXT(#x) TEXT("m")
#define ANSI_RESET ESC TEXT("[0m")

void DisplayWelcomeMessage(bool Ansi)
{
    const HMODULE	Module = GetModuleHandle(NULL);
    DisplayAboutMessage(Module, TEXT("RadSync"));

    _tprintf(_T("\nOne way sync between two directories.\n\nRadSync <options> [src] [dst]\n\nOptions:\n"));
    TCHAR* options[][2] = {
        { TEXT("t"),  TEXT("Test - do not change anything") },
        { TEXT("h"),  TEXT("Hidden - copy hidden files as well") },
        { TEXT("d"),  TEXT("Delete - delete files in destination") },
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

struct Options
{
    Options()
        : nf(LOCALE_USER_DEFAULT)
        , test(false)
        , hidden(false)
        , dodelete(false)
    {
    }

    NumberFormat nf;
    bool test;
    bool hidden;
    bool dodelete;
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

void SyncDeleteFileDir(const CDirectory::CEntry& source_entry, const Url& destination, const Options& options)
{
    if (source_entry.IsDirectory())
    {
        //_tprintf(_T("Delete directory %s from %s to %s\n"), source_entry.GetFileName(), source.GetUrl(), destination.GetUrl());
        if (!options.test && options.dodelete)
        {
            Url subdirdest = destination + source_entry.GetFileName();

            std::vector<CDirectory::CEntry> dirlist;
            subdirdest.AppendDelim();
            GetDirectory(subdirdest, dirlist, true);

            DirSorter dir_sorter(DirSorter::Order::Name, true);
            SortDirectory(dirlist, dir_sorter);

            for (std::vector<CDirectory::CEntry>::const_iterator dirlist_it = dirlist.begin(); dirlist_it != dirlist.end(); ++dirlist_it)
            {
                if (!dirlist_it->IsDots())
                {
                    //_tprintf(_T(" X %s\n"), dirlist_it->GetFileName());
                    SyncDeleteFileDir(*dirlist_it, subdirdest, options);
                }
            }

            subdirdest.DeleteDirectory();
        }
    }
    else
    {
        //_tprintf(_T("Delete file %s from %s to %s\n"), source_entry.GetFileName(), source.GetUrl(), destination.GetUrl());
        if (!options.test && options.dodelete)
        {
            DeleteFile(destination, source_entry);
        }
    }
}

void SyncDirectories(const Url& dirA, const std::vector<CDirectory::CEntry>& dirlistA, const Url& dirB, const std::vector<CDirectory::CEntry>& dirlistB, const DirSorter& dir_sorter, const Options& options)
{
    try
    {
        std::vector<CDirectory::CEntry>::const_iterator dirlistA_it = dirlistA.begin();
        std::vector<CDirectory::CEntry>::const_iterator dirlistB_it = dirlistB.begin();

        while (dirlistA_it != dirlistA.end() || dirlistB_it != dirlistB.end())
        {
            if (dirlistA_it != dirlistA.end() && dirlistA_it->IsDots())
                ++dirlistA_it;
            else if (dirlistB_it != dirlistB.end() && dirlistB_it->IsDots())
                ++dirlistB_it;
            else
            {
                if (dirlistA_it == dirlistA.end() || dir_sorter.Compare(*dirlistB_it, *dirlistA_it))
                {
                    if (options.hidden || !dirlistB_it->IsHidden())
                    {
                        _tprintf(_T(" X %s\n"), dirlistB_it->GetFileName());
                        try
                        {
                            SyncDeleteFileDir(*dirlistB_it, dirB, options);
                        }
                        catch (const rad::WinError& e)
                        {
                            _ftprintf(stderr, e.GetString().c_str());
                        }
                    }
                    ++dirlistB_it;
                }
                else if (dirlistB_it == dirlistB.end() || dir_sorter.Compare(*dirlistA_it, *dirlistB_it))
                {
                    if (options.hidden || !dirlistA_it->IsHidden())
                    {
                        _tprintf(_T(" > %s\n"), dirlistA_it->GetFileName());
                        try
                        {
                            SyncCopyFileDir(dirA, *dirlistA_it, dirB, options);
                        }
                        catch (const rad::WinError& e)
                        {
                            _ftprintf(stderr, e.GetString().c_str());
                        }
                    }
                    ++dirlistA_it;
                }
                else
                {
                    assert(_tcscmp(dirlistA_it->GetFileName(), dirlistB_it->GetFileName()) == 0);
                    assert(dirlistA_it->IsDirectory() == dirlistB_it->IsDirectory());
                    if (!dirlistA_it->IsDirectory()) //&& !dirlistB_it->IsDirectory())
                    {
                        if (options.hidden || !dirlistA_it->IsHidden())
                        {
                            if (dirlistA_it->GetFileSize().QuadPart != dirlistB_it->GetFileSize().QuadPart
                                || dirlistA_it->ftLastWriteTime != dirlistB_it->ftLastWriteTime)
                            {
                                if (dirlistA_it->ftLastWriteTime > dirlistB_it->ftLastWriteTime)
                                {
                                    _tprintf(_T(" > %s\n"), dirlistA_it->GetFileName());
                                    if (!options.test)
                                    {
                                        try
                                        {
                                            CopyFile(dirA, dirB, *dirlistA_it, &options.nf);
                                        }
                                        catch (const rad::WinError& e)
                                        {
                                            _ftprintf(stderr, e.GetString().c_str());
                                        }
                                    }
                                }
                                else if (dirlistA_it->GetFileSize().QuadPart != dirlistB_it->GetFileSize().QuadPart)
                                {
                                    _tprintf(_T(" ! %s\n"), dirlistA_it->GetFileName());
                                    _tprintf(_T("Warning: File %s different size but destination has later time.\n"), dirlistA_it->GetFileName());
                                }
                                //else
                                //_tprintf(_T(" = %s\n"), dirlistA_it->GetFileName());
                            }
                            //else
                            //_tprintf(_T(" = %s\n"), dirlistA_it->GetFileName());
                        }
                    }
                    else //if (dirlistA_it->IsDirectory() && dirlistB_it->IsDirectory())
                    {
                        SyncDirectories(dirA + dirlistA_it->GetFileName(), dirB + dirlistB_it->GetFileName(), options);
                    }
                    ++dirlistA_it;
                    ++dirlistB_it;
                }
            }
        }
    }
    catch (const rad::WinError& e)
    {
        // TODO This ends syncing the directory
        // This should be caught for each for update
        _ftprintf(stderr, e.GetString().c_str());
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

    DirSorter dir_sorter(DirSorter::Order::Name, true);
    SortDirectory(dirlistA, dir_sorter);
    SortDirectory(dirlistB, dir_sorter);

    SyncDirectories(dirA, dirlistA, dirB, dirlistB, dir_sorter, options);
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
        const bool Ansi = _isatty(_fileno(stdout));
        Options options;
        std::vector<const TCHAR*> my_args;

        BoolArg boolArg[] =
        {
            { TEXT('t'), &options.test },
            { TEXT('h'), &options.hidden },
            { TEXT('d'), &options.dodelete },
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
                while (thisBoolArg->m_Char != _totlower(argv[arg][Pos]) && thisBoolArg->m_Char != TEXT('\0'))
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
            DisplayWelcomeMessage(Ansi);
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
