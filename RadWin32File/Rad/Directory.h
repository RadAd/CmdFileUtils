#ifndef DIRECTORY_H
#define DIRECTORY_H

#include <Windows.h>
#include <string>
#include <TChar.h>
#include <Rad/tstring.h>

class CDirectory
{
public:
    class CEntry : public WIN32_FIND_DATA
    {
    public:
        const TCHAR *GetFileName() const { return cFileName; }
        const TCHAR *GetFileExt() const { return _tcsrchr(cFileName, _T('.')); }
        size_t GetFileNameLen() const { return _tcsclen(cFileName); }
        ULARGE_INTEGER GetFileSize() const { ULARGE_INTEGER s; s.LowPart = nFileSizeLow; s.HighPart = nFileSizeHigh; return s; }
        bool IsDots() const { return _tcscmp(GetFileName(), _T(".")) == 0 || _tcscmp(GetFileName(), _T("..")) == 0; }
        bool IsReadOnly() const { return (dwFileAttributes & FILE_ATTRIBUTE_READONLY) != 0; }
        bool IsHidden() const { return (dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) != 0; }
        bool IsSystem() const { return (dwFileAttributes & FILE_ATTRIBUTE_SYSTEM) != 0; }
        bool IsDirectory() const { return (dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0; }
        bool IsArchive() const { return (dwFileAttributes & FILE_ATTRIBUTE_ARCHIVE) != 0; }
        bool IsEncrypted() const { return (dwFileAttributes & FILE_ATTRIBUTE_ENCRYPTED) != 0; }
        bool IsNormal() const { return (dwFileAttributes & FILE_ATTRIBUTE_NORMAL) != 0; }
        bool IsTemporary() const { return (dwFileAttributes & FILE_ATTRIBUTE_TEMPORARY) != 0; }
        bool IsSparseFile() const { return (dwFileAttributes & FILE_ATTRIBUTE_SPARSE_FILE) != 0; }
        bool IsReparsePoint() const { return (dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0; }
        bool IsCompressed() const { return (dwFileAttributes & FILE_ATTRIBUTE_COMPRESSED) != 0; }
        bool IsOffline() const { return (dwFileAttributes & FILE_ATTRIBUTE_OFFLINE) != 0; }
        bool IsNotContentIndexed() const { return (dwFileAttributes & FILE_ATTRIBUTE_NOT_CONTENT_INDEXED) != 0; }
    };

    CDirectory(const std::tstring& dir, WIN32_FIND_DATA& Entry)
    {
        m_Dir = FindFirstFile(dir.c_str(), &Entry);
    }

    ~CDirectory() { if (m_Dir) FindClose(m_Dir); }

    bool IsValid() const { return (m_Dir != INVALID_HANDLE_VALUE); }

    bool NextFile(WIN32_FIND_DATA& Entry) { return FindNextFile(m_Dir, &Entry) != FALSE; }

    static TCHAR GetDelim() { return _T('\\'); }
    static void AppendDelim(std::tstring& path) { if (path.at(path.length() - 1) != GetDelim()) path += GetDelim(); }
    static bool Exists(std::tstring dir_name)
    {
        {	// remove the trailing delimeter
            std::tstring::iterator it = dir_name.end();
            if (it != dir_name.begin())
            {
                --it;
                if (*it == GetDelim())
                    dir_name.erase(it);
            }
        }
        CDirectory::CEntry	dir_entry;
        CDirectory			dir(dir_name, dir_entry);
        if (dir.IsValid())
            return dir_entry.IsDirectory();
        else
            return false;
    }

private:
    HANDLE		m_Dir;
};

#endif
