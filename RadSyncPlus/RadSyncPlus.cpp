#include <assert.h>

#include <Rad/WinError.H>
#include <Rad/About/AboutMessage.h>
#include <Rad/ConsoleUtils.h>
#include <Rad/DirHelper.h>
#include <Rad/NumberFormat.h>

class CUserError
{
private:
    std::tstring m_UserMessage;
    rad::WinError m_Error;
public:

    CUserError(const std::tstring& msg, const rad::WinError& e)
        : m_UserMessage(msg)
        , m_Error(e)
    {
    }

    const std::tstring& GetString() const { return m_UserMessage; }
};

void DisplayWelcomeMessage()
{
    const HMODULE    Module = GetModuleHandle(NULL);
    DisplayAboutMessage(Module, TEXT("RadSyncPlus"));

    const HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    const WORD OriginalAttribute = GetConsoleTextAttribute(hOut);
    const WORD HiliteAttribute = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;

    _tprintf(_T("\nTwo way sync between two directories.\n\nRadSync <options> [dir1]\nRadSync [destdir] [srcdir]\n\nOptions:\n"));
    TCHAR* options[][2] = {
        { TEXT("t"),  TEXT("Test - do not change anything") },
        // { TEXT("h"),  TEXT("Hidden - copy hidden files as well") }, TODO
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
        // , hidden(false)
    {
    }

    NumberFormat nf;
    bool test;
    // bool hidden;
};

void CreateSync(const Url& backup, std::tstring orig);
void SyncDirectory(Url backup, const Options& options);

void RecurseRemoveDirectory(const Url& dir)
{
    std::vector<CDirectory::CEntry> dirlist;
    GetDirectory(dir, dirlist, true);

    std::vector<CDirectory::CEntry>::const_iterator dirlist_it;
    for (dirlist_it = dirlist.begin(); dirlist_it != dirlist.end(); ++dirlist_it)
    {
        if (!dirlist_it->IsDots())
        {
            Url subfiledir(dir + dirlist_it->GetFileName());
            if (dirlist_it->IsDirectory())
                RecurseRemoveDirectory(subfiledir);
            else
                subfiledir.DeleteFile();
        }
    }

    dir.DeleteDirectory();
}

void SyncCopyFileDir(const Url& source, const CDirectory::CEntry& source_entry, const Url& destination, bool create_sync, const Options& options)
{
    if (source_entry.IsDirectory())
    {
        _tprintf(_T("Copy directory %s from %s to %s\n"), source_entry.GetFileName(), source.GetUrl(), destination.GetUrl());
        if (!options.test)
        {
            Url sub_destination(destination + source_entry.GetFileName());
            Url sub_source(source + source_entry.GetFileName());

            sub_destination.CreateDirectory();
            if (create_sync)
            {
                CreateSync(sub_destination, sub_source.GetUrl());
                SyncDirectory(sub_destination, options);
            }
            else
            {
                CreateSync(sub_source, sub_destination.GetUrl());
                SyncDirectory(sub_source, options);
            }
        }
    }
    else
    {
        _tprintf(_T("Copy file %s from %s to %s\n"), source_entry.GetFileName(), source.GetUrl(), destination.GetUrl());
        if (!options.test)
        {
            CopyFile(source, destination, source_entry, &options.nf);
        }
    }
}

void SyncDeleteFileDir(const Url& source, const CDirectory::CEntry& source_entry, const Options& options)
{
    if (source_entry.IsDirectory())
    {
        _tprintf(_T("Delete directory %s from %s\n"), source_entry.GetFileName(), source.GetUrl());
        if (!options.test)
        {
            RecurseRemoveDirectory(source + source_entry.GetFileName());
        }
    }
    else
    {
        _tprintf(_T("Delete file %s from %s\n"), source_entry.GetFileName(), source.GetUrl());
        if (!options.test)
        {
            Url source_file(source + source_entry.GetFileName());
            source_file.DeleteFile();
        }
    }
}

inline bool Same(const CDirectory::CEntry& left, const CDirectory::CEntry& right)
{
    assert(!left.IsDirectory() && !right.IsDirectory());
    if (left.ftLastWriteTime != right.ftLastWriteTime)
        return false;
    else
    {
        if (left.nFileSizeHigh == right.nFileSizeHigh
            && left.nFileSizeLow == right.nFileSizeLow)
            return true;
        else
        {
            _tprintf(_T("Warning: File %s different size but same time.\n"), left.GetFileName());
            return false;
        }
    }
}

inline int NameCompare(std::vector<CDirectory::CEntry>::const_iterator left_it, std::vector<CDirectory::CEntry>::const_iterator left_end_it,
                       std::vector<CDirectory::CEntry>::const_iterator right_it, std::vector<CDirectory::CEntry>::const_iterator right_end_it)
{
    if (left_it == left_end_it)
        return 1;
    else if (right_it == right_end_it)
        return -1;
    else
    {
        if (left_it->IsDirectory() == right_it->IsDirectory())
            return _tcsicmp(left_it->GetFileName(), right_it->GetFileName());
        else
        {
            if (left_it->IsDirectory())
                return -1;
            else
                return 1;
        }
    }
}

void SyncDirectories(const Url& backup, const std::vector<CDirectory::CEntry>& dirlistbackup, const Url& orig, const std::vector<CDirectory::CEntry>& dirlistorig, std::vector<CDirectory::CEntry>& dirlistsaved, const Options& options)
{
    std::vector<CDirectory::CEntry>::const_iterator dirlistbackup_it = dirlistbackup.begin();
    std::vector<CDirectory::CEntry>::const_iterator dirlistorig_it = dirlistorig.begin();
    std::vector<CDirectory::CEntry>::iterator dirlistsaved_it = dirlistsaved.begin();

    while (dirlistbackup_it != dirlistbackup.end() || dirlistorig_it != dirlistorig.end())
    {
        if (dirlistbackup_it != dirlistbackup.end() && dirlistbackup_it->IsDots())
            ++dirlistbackup_it;
        else if (dirlistbackup_it != dirlistbackup.end() && dirlistbackup_it->GetFileExt() && _tcscmp(dirlistbackup_it->GetFileExt(), TEXT(".radsync")) == 0)
            ++dirlistbackup_it;
        else if (dirlistorig_it != dirlistorig.end() && dirlistorig_it->IsDots())
            ++dirlistorig_it;
        else if (dirlistorig_it != dirlistorig.end() && dirlistorig_it->GetFileExt() && _tcscmp(dirlistorig_it->GetFileExt(), TEXT(".radsync")) == 0)
            ++dirlistorig_it;
        else if (dirlistsaved_it != dirlistsaved.end() && dirlistsaved_it->IsDots())
            ++dirlistsaved_it;
        else if (dirlistsaved_it != dirlistsaved.end() && dirlistsaved_it->GetFileExt() && _tcscmp(dirlistsaved_it->GetFileExt(), TEXT(".radsync")) == 0)
            ++dirlistsaved_it;
        else
        {
            int compare_bo = NameCompare(dirlistbackup_it, dirlistbackup.end(), dirlistorig_it, dirlistorig.end());
            int compare_bs = NameCompare(dirlistbackup_it, dirlistbackup.end(), dirlistsaved_it, dirlistsaved.end());
            int compare_os = NameCompare(dirlistorig_it, dirlistorig.end(), dirlistsaved_it, dirlistsaved.end());

            const CDirectory::CEntry* the_orig = dirlistorig.end() == dirlistorig_it ? 0 : &*dirlistorig_it;
            const CDirectory::CEntry* the_backup = dirlistbackup.end() == dirlistbackup_it ? 0 : &*dirlistbackup_it;
            CDirectory::CEntry* the_saved = dirlistsaved.end() == dirlistsaved_it ? 0 : &*dirlistsaved_it;

            try
            {
                if (compare_bo > 0)
                {
                    the_backup = 0;
                }
                else if (compare_bo < 0)
                {
                    the_orig = 0;
                }

                if (compare_bs > 0)
                {
                    the_backup = 0;
                }
                else if (compare_bs < 0)
                {
                    the_saved = 0;
                }

                if (compare_os > 0)
                {
                    the_orig = 0;
                }
                else if (compare_os < 0)
                {
                    the_saved = 0;
                }

                if (the_saved != 0 && the_backup != 0 && the_saved->IsDirectory() != the_backup->IsDirectory())
                {
                    _tprintf(_T("Warning: File %s shares name with a directory.\n"), the_saved->GetFileName());
                }
                else if (the_saved == 0 && the_orig == 0)
                {
                    //_tprintf(_T("%s - Copy from backup to orig. Add to saved.\n"), the_backup->GetFileName());
                    SyncCopyFileDir(backup, *the_backup, orig, false, options);
                    if (!options.test)
                    {
                        dirlistsaved_it = dirlistsaved.insert(dirlistsaved_it, *the_backup);
                        ++dirlistsaved_it;
                    }
                }
                else if (the_backup == 0 && the_orig == 0)
                {
                    //_tprintf(_T("%s - Problem? Remove from saved.\n"), the_saved->GetFileName());
                    if (!options.test)
                    {
                        dirlistsaved_it = dirlistsaved.erase(dirlistsaved_it);
                        the_saved = 0;
                    }
                }
                else if (the_saved == 0 && the_backup == 0)
                {
                    //_tprintf(_T("%s - Copy from orig to backup. Add to saved.\n"), the_orig->GetFileName());
                    SyncCopyFileDir(orig, *the_orig, backup, true, options);
                    if (!options.test)
                    {
                        dirlistsaved_it = dirlistsaved.insert(dirlistsaved_it, *the_orig);
                        ++dirlistsaved_it;
                    }
                }
                else if (the_orig == 0)
                {
                    if (the_backup->IsDirectory())
                    {
                        // TODO assume the_saved->IsDirectory()
                        //_tprintf(_T("%s - Delete from orig, Remove from saved.\n"), the_saved->GetFileName());
                        _tprintf(_T("Delete directory %s from %s\n"), the_backup->GetFileName(), backup.GetUrl());
                        if (!options.test)
                        {
                            RecurseRemoveDirectory(backup + the_backup->GetFileName());
                            dirlistsaved_it = dirlistsaved.erase(dirlistsaved_it);
                            the_saved = 0;
                        }
                    }
                    else if (Same(*the_backup, *the_saved))
                    {
                        //_tprintf(_T("%s - Delete from backup. Remove from saved.\n"), the_saved->GetFileName());
                        SyncDeleteFileDir(backup, *the_backup, options);
                        if (!options.test)
                        {
                            dirlistsaved_it = dirlistsaved.erase(dirlistsaved_it);
                            the_saved = 0;
                        }
                    }
                    else
                        _tprintf(_T("%s - This file has been deleted from %s but has been edited in %s. Nothing updated.\n"), the_saved->GetFileName(), orig.GetUrl(), backup.GetUrl());
                }
                else if (the_backup == 0)
                {
                    if (the_orig->IsDirectory())
                    {
                        // TODO assume the_saved->IsDirectory()
                        //_tprintf(_T("%s - Delete from orig, Remove from saved.\n"), the_saved->GetFileName());
                        _tprintf(_T("Delete directory %s from %s\n"), the_orig->GetFileName(), orig.GetUrl());
                        if (!options.test)
                        {
                            RecurseRemoveDirectory(orig + the_orig->GetFileName());
                            dirlistsaved_it = dirlistsaved.erase(dirlistsaved_it);
                            the_saved = 0;
                        }
                    }
                    else if (Same(*the_orig, *the_saved))
                    {
                        //_tprintf(_T("%s - Delete from orig, Remove from saved.\n"), the_saved->GetFileName());
                        SyncDeleteFileDir(orig, *the_orig, options);
                        if (!options.test)
                        {
                            dirlistsaved_it = dirlistsaved.erase(dirlistsaved_it);
                            the_saved = 0;
                        }
                    }
                    else
                        _tprintf(_T("%s - This file has been deleted from %s but has been edited in %s. Nothing updated.\n"), the_saved->GetFileName(), backup.GetUrl(), orig.GetUrl());
                }
                else if (the_saved == 0)
                {
                    if (the_backup->IsDirectory())
                    {
                        // TODO assume the_orig->IsDirectory()
                        //_tprintf(_T("%s - Update saved with orig (or backup - both the same).\n"), the_orig->GetFileName());
                        CreateSync(backup + the_backup->GetFileName(), (orig + the_backup->GetFileName()).GetUrl());
                        SyncDirectory(backup + the_backup->GetFileName(), options);
                        if (!options.test)
                        {
                            dirlistsaved_it = dirlistsaved.insert(dirlistsaved_it, *the_orig);
                            ++dirlistsaved_it;
                        }
                    }
                    else if (Same(*the_backup, *the_orig))
                    {
                        //_tprintf(_T("%s - Update saved with orig (or backup - both the same).\n"), the_orig->GetFileName());
                        if (!options.test)
                        {
                            dirlistsaved_it = dirlistsaved.insert(dirlistsaved_it, *the_orig);
                            ++dirlistsaved_it;
                        }
                    }
                    else
                        _tprintf(_T("%s in %s and %s - Both created. Nothing updated.\n"), the_orig->GetFileName(), backup.GetUrl(), orig.GetUrl());
                }
                else
                {
                    if (the_backup->IsDirectory())
                    {
                        // TODO assume the_orig->IsDirectory() and the_saved->IsDirectory()
                        SyncDirectory(backup + the_backup->GetFileName(), options);
                        if (!options.test)
                        {
                            *the_saved = *the_backup;
                        }
                    }
                    else if (Same(*the_backup, *the_orig))
                    {
                        if (Same(*the_backup, *the_saved))
                        {
                            if (Same(*the_orig, *the_saved))
                            {
                                //_tprintf(_T("%s - Nothing to do - all the same.\n"), the_orig->GetFileName());
                            }
                            else
                            {
                                _tprintf(_T("%s - Error should never happen.\n"), the_orig->GetFileName());
                            }
                        }
                        else
                        {
                            if (Same(*the_orig, *the_saved))
                            {
                                _tprintf(_T("%s - Error should never happen.\n"), the_orig->GetFileName());
                            }
                            else
                            {
                                //_tprintf(_T("%s - Just update the save.\n"), the_orig->GetFileName());
                                if (!options.test)
                                {
                                    *the_saved = *the_orig;
                                }
                            }
                        }
                    }
                    else
                    {
                        if (Same(*the_backup, *the_saved))
                        {
                            if (Same(*the_orig, *the_saved))
                            {
                                _tprintf(_T("%s - Error should never happen.\n"), the_orig->GetFileName());
                            }
                            else
                            {
                                //_tprintf(_T("%s - Copy orig to backup. Update saved with orig.\n"), the_orig->GetFileName());
                                SyncCopyFileDir(orig, *the_orig, backup, true, options);
                                if (!options.test)
                                {
                                    *the_saved = *the_orig;
                                }
                            }
                        }
                        else
                        {
                            if (Same(*the_orig, *the_saved))
                            {
                                //_tprintf(_T("%s - Copy backup to orig. Update saved with backup.\n"), the_orig->GetFileName());
                                SyncCopyFileDir(backup, *the_backup, orig, false, options);
                                if (!options.test)
                                {
                                *the_saved = *the_backup;
                                }
                            }
                            else
                            {
                                _tprintf(_T("%s in %s and %s - Both changed. Nothing updated.\n"), the_orig->GetFileName(), backup.GetUrl(), orig.GetUrl());
                            }
                        }
                    }
                }
            }
            catch(const CUserError &e)
            {
                _ftprintf(stderr, e.GetString().c_str());
            }

            if (the_saved)
                ++dirlistsaved_it;
            if (the_orig)
                ++dirlistorig_it;
            if (the_backup)
                ++dirlistbackup_it;
        }
    }
}

WIN32_FIND_DATAA& Copy(WIN32_FIND_DATAA& l, const WIN32_FIND_DATAW& r)
{
    l.dwFileAttributes = r.dwFileAttributes;
    l.ftCreationTime = r.ftCreationTime;
    l.ftLastAccessTime = r.ftLastAccessTime;
    l.ftLastWriteTime = r.ftLastWriteTime;
    l.nFileSizeHigh = r.nFileSizeHigh;
    l.nFileSizeLow = r.nFileSizeLow;
    l.dwReserved0 = r.dwReserved0;
    l.dwReserved1 = r.dwReserved1;
    size_t converted;
    wcstombs_s(&converted, l.cFileName, r.cFileName, MAX_PATH);
    wcstombs_s(&converted, l.cAlternateFileName, r.cAlternateFileName, 14);
    return l;
}

WIN32_FIND_DATAW& Copy(WIN32_FIND_DATAW& l, const WIN32_FIND_DATAA& r)
{
    l.dwFileAttributes = r.dwFileAttributes;
    l.ftCreationTime = r.ftCreationTime;
    l.ftLastAccessTime = r.ftLastAccessTime;
    l.ftLastWriteTime = r.ftLastWriteTime;
    l.nFileSizeHigh = r.nFileSizeHigh;
    l.nFileSizeLow = r.nFileSizeLow;
    l.dwReserved0 = r.dwReserved0;
    l.dwReserved1 = r.dwReserved1;
    size_t converted;
    mbstowcs_s(&converted, l.cFileName, r.cFileName, MAX_PATH);
    mbstowcs_s(&converted, l.cAlternateFileName, r.cAlternateFileName, 14);
    return l;
}

void SyncDirectory(Url backup, const Options& options)
{
    backup.AppendDelim();

    TCHAR orig_url[MAX_PATH];
    short entry_size = 0;
    short count = 0;

    std::vector<CDirectory::CEntry> dirlistsaved;
    {
        std::auto_ptr<FileRead> file;
        try
        {
            file = std::auto_ptr<FileRead>((backup + TEXT("database.radsync")).OpenFileRead());
        }
        catch(const rad::WinError& e)
        {
            throw CUserError(std::tstring(TEXT("Not a RadSyncPlus directory: ")) + backup.GetUrl(), e);
        }

        TCHAR convertorig[MAX_PATH];
        file->Read(convertorig, MAX_PATH);
        file->Read(&entry_size, sizeof(entry_size));
        file->Read(&count, sizeof(count));

        if (entry_size > 0)
        {
            if (entry_size != sizeof(CDirectory::CEntry))
            {
                size_t converted;
#if UNICODE
                typedef WIN32_FIND_DATAA WIN32_FIND_DATAC;
                mbstowcs_s(&converted, orig_url, (const char*) convertorig, MAX_PATH);
#else
                typedef WIN32_FIND_DATAW WIN32_FIND_DATAC;
                wcstombs_s(&converted, orig_url, (const wchar_t*) convertorig, MAX_PATH);
#endif
                std::vector<WIN32_FIND_DATAC> convertdirlistsaved;
                assert(entry_size == sizeof(WIN32_FIND_DATAC));
                assert(sizeof(WIN32_FIND_DATA) == sizeof(CDirectory::CEntry));

                convertdirlistsaved.resize(count);
                file->Read(&convertdirlistsaved.front(), count * sizeof(WIN32_FIND_DATAC));

                dirlistsaved.resize(count);
                for (int i = 0; i < (int) convertdirlistsaved.size(); ++i)
                    Copy(dirlistsaved[i], convertdirlistsaved[i]);

                entry_size = sizeof(CDirectory::CEntry);
            }
            else
            {
                _tcscpy_s(orig_url, convertorig);
                assert(entry_size == sizeof(CDirectory::CEntry));
                assert(sizeof(WIN32_FIND_DATA) == sizeof(CDirectory::CEntry));

                dirlistsaved.resize(count);
                if (count)
                    file->Read(&dirlistsaved.front(), count * sizeof(CDirectory::CEntry));
            }
        }
    }

    Url orig(orig_url);

    std::vector<CDirectory::CEntry> dirlistbackup;
    std::vector<CDirectory::CEntry> dirlistorig;

    GetDirectory(backup, dirlistbackup, false);
    GetDirectory(orig, dirlistorig, false);

    DirSorter dir_sorter(DirSorter::Name, true);
    SortDirectory(dirlistbackup, dir_sorter);
    SortDirectory(dirlistorig, dir_sorter);
    SortDirectory(dirlistsaved, dir_sorter);

    SyncDirectories(backup, dirlistbackup, orig, dirlistorig, dirlistsaved, options);

    if (!options.test)
    {
        std::auto_ptr<FileWrite> file((backup + TEXT("database.radsync")).OpenFileWrite(-1, true));

        count = (short) dirlistsaved.size();

        file->Write(orig.GetUrl(), MAX_PATH);
        file->Write(&entry_size, sizeof(entry_size));
        file->Write(&count, sizeof(count));

        assert(entry_size == sizeof(CDirectory::CEntry));
        assert(sizeof(WIN32_FIND_DATA) == sizeof(CDirectory::CEntry));

        if (count)
            file->Write(&dirlistsaved.front(), count * sizeof(CDirectory::CEntry));
    }
}

void CreateSync(const Url& backup, std::tstring orig)
{
    if (orig.compare(0, 4, TEXT("ftp:")) != 0)
        CDirectory::AppendDelim(orig);

    Url database(backup + TEXT("database.radsync"));
    std::auto_ptr<FileWrite> file(database.OpenFileWrite(-1, true));

    short entry_size = sizeof(CDirectory::CEntry);
    short count = 0;

    TCHAR orig_str[MAX_PATH];
    _tcscpy_s(orig_str, orig.c_str());
    file->Write(orig_str, MAX_PATH);
    file->Write(&entry_size, sizeof(entry_size));
    file->Write(&count, sizeof(count));
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
            { TEXT('t'), &options.test},
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

        if (my_args.size() >= 2)
            CreateSync(my_args[0], my_args[1]);

        if (my_args.size() >= 1)
            SyncDirectory(my_args[0], options);
        else
            DisplayWelcomeMessage();
    }
    catch(const rad::WinError& e)
    {
        _ftprintf(stderr, e.GetString().c_str());
        return EXIT_FAILURE + 0;
    }
    catch(const CUserError &e)
    {
        _ftprintf(stderr, e.GetString().c_str());
        return EXIT_FAILURE + 2;
    }
    catch(...)
    {
        _ftprintf(stderr, TEXT("Unknown Exception\n"));
        return EXIT_FAILURE + 1;
    }

    return EXIT_SUCCESS;
}
