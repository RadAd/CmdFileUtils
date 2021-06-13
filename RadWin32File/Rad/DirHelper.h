#ifndef DIRHELPER_H
#define DIRHELPER_H

#include <vector>

#include "Directory.H"
#include "Win\WinFile.h"
#include "Win\WinInetFile.h"

class FileRead
{
public:
    virtual ~FileRead() { }
    virtual DWORD Read(void *Buffer, const DWORD Size) = 0;
    virtual ULARGE_INTEGER GetSize() const = 0;
};

class FileWrite
{
public:
    virtual ~FileWrite() { }
    virtual DWORD Write(void const *Buffer, const DWORD Size) = 0;
};

void CopyFile(CWinInetFile& IFile, CWinFile& OFile, CWinInetFile::FileSizeT FileSize, const NUMBERFMT* nf, bool Quiet);

class Url : public URL_COMPONENTS
{
public:
    /*explicit*/ Url(const TCHAR *url) { Init(url); }
    /*explicit*/ Url(const std::tstring& url) { Init(url.c_str()); }
    Url(const Url& other) { Init(other.m_Url); }

    void Init(const TCHAR *url);
    void Change(const TCHAR *subdir);
    void Change(const std::tstring& subdir) { Change(subdir.c_str()); }

    TCHAR GetDelim() const
    {
        if (nScheme == INTERNET_SCHEME_FTP || nScheme == INTERNET_SCHEME_HTTP)
            return '/';
        else
            return '\\';
    }
    void AppendDelim();
    const TCHAR* GetHost() const { return m_Host; }
    INTERNET_PORT GetPort() const { return nPort; }
    const TCHAR* GetUser() const { return m_User; }
    const TCHAR* GetPassword() const { return m_Password; }
    const TCHAR* GetPath() const { return m_Path; }
    const TCHAR* GetScheme() const { return lpszScheme; }
    const TCHAR* GetUrl() const { return m_Url; }

    void SetTime(const FILETIME* lpCreationTime, const FILETIME* lpLastAccessTime, const FILETIME* lpLastWriteTime) const;
    void DeleteFile() const;
    void CreateDirectory() const;
    void DeleteDirectory() const;
    void RemoveAttr(DWORD RemAttr) const;

    CWinInetHandle Connect() const;
    std::auto_ptr<FileRead> OpenFileRead() const;
    std::auto_ptr<FileWrite> OpenFileWrite(int buffer_size, bool Hidden) const;
    bool IsLocal() const { return nScheme == INTERNET_SCHEME_UNKNOWN || nScheme == INTERNET_SCHEME_DEFAULT; }
    bool GetLocally(const TCHAR* destination, const NUMBERFMT* nf) const;
    bool PutLocally(const TCHAR* source, const NUMBERFMT* nf) const;

private:
    TCHAR m_Url[1024];
    TCHAR m_Scheme[100];
    TCHAR m_Host[100];
    TCHAR m_User[100];
    TCHAR m_Password[100];
    TCHAR m_Path[1024];
};

void CopyFile(const Url& source, const Url& destination, const CDirectory::CEntry& entry, const NUMBERFMT* nf);
void DeleteFile(const Url& destination, const CDirectory::CEntry& entry);

inline Url operator+(const Url& url, const std::tstring& subdir)
{
    Url sub_url(url);
    sub_url.Change(subdir);
    return sub_url;
}

void GetDirectory(const Url& url, std::vector<CDirectory::CEntry>& dirlist, bool ShowHidden);

class DirSorter
{
public:
    enum class Order { None, Name, Size, Date };

    DirSorter(Order order, bool DirFirst)
        : m_Order(order), m_DirFirst(DirFirst)
    {
    }

    bool operator()(const CDirectory::CEntry& left, const CDirectory::CEntry& right) const;
    bool Compare(const CDirectory::CEntry& left, const CDirectory::CEntry& right) const;
    static bool CompareName(const CDirectory::CEntry& left, const CDirectory::CEntry& right);
    static bool CompareSize(const CDirectory::CEntry& left, const CDirectory::CEntry& right);
    static bool CompareDate(const CDirectory::CEntry& left, const CDirectory::CEntry& right);

private:
    Order m_Order;
    bool m_DirFirst;
};

void SortDirectory(std::vector<CDirectory::CEntry>& dirlist, const DirSorter& sorter);

inline bool operator>(const FILETIME& a, const FILETIME& b)
{
    return CompareFileTime(&a, &b) > 0;
}

inline bool operator>=(const FILETIME& a, const FILETIME& b)
{
    return CompareFileTime(&a, &b) >= 0;
}

inline bool operator<(const FILETIME& a, const FILETIME& b)
{
    return CompareFileTime(&a, &b) < 0;
}

inline bool operator<=(const FILETIME& a, const FILETIME& b)
{
    return CompareFileTime(&a, &b) <= 0;
}

inline bool operator==(const FILETIME& a, const FILETIME& b)
{
    //return CompareFileTime(&a, &b) == 0;
    //return a.dwLowDateTime == b.dwLowDateTime && a.dwHighDateTime == b.dwHighDateTime;
    ULARGE_INTEGER la;
    la.HighPart = a.dwHighDateTime;
    la.LowPart = a.dwLowDateTime;
    ULARGE_INTEGER lb;
    lb.HighPart = b.dwHighDateTime;
    lb.LowPart = b.dwLowDateTime;

    // 2 seconds = 10,000,000 100-nanseconds = 2,000,000,000 nan0seconds
    #define _SECOND ((ULONGLONG) 10000000)

    if (la.QuadPart > lb.QuadPart)
        return (la.QuadPart - lb.QuadPart) < (2 * _SECOND);
    else
        return (lb.QuadPart - la.QuadPart) < (2 * _SECOND);
}

inline bool operator!=(const FILETIME& a, const FILETIME& b)
{
    return !(a == b);
}

#endif
