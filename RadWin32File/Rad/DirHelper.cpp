#define _WIN32_WINNT 0x0400

#include "DirHelper.h"

#include <string>
#include <algorithm>
#include <TChar.H>
#include <WinCred.H>
#include <assert.h>

#include "Win/WinInetHandle.h"
#include "Win/WinInetFile.h"
#include "Win/WinFile.H"

#include "NumberFormat.h"

ULARGE_INTEGER ToFileSize(ULONGLONG s)
{
    ULARGE_INTEGER size;
    size.QuadPart = s;
    return size;
}

class DrawProgress
{
public:
    DrawProgress(const NUMBERFMT* nf)
        : m_nf(nf)
    {
        //out = stdout;
        fopen_s(&out, "CONOUT$", "w");
        //Draw(ToFileSize(0), ToFileSize(100));
    }

    ~DrawProgress()
    {
        //_ftprintf(out, _T("%*s\n"), 79, _T(""));
        _ftprintf(out, L"\x1b]9;4;0;0\x1b\\");
        _ftprintf(out, L"\x1b[2K");
        fclose(out);
    }

    void Draw(CWinInetFile::FileSizeT Transferred, CWinInetFile::FileSizeT FileSize)
    {
        if (FileSize.QuadPart > 0 && FileSize.QuadPart < INT64_MAX)
        {
            double percent = (double) Transferred.QuadPart / FileSize.QuadPart;
            if (percent > 1.0)
                percent = 1.0;

#if 0
            TCHAR title[256];
            _stprintf_s(title, _T("RadGeturl: %7.2f%%"), percent * 100.0);  // TODO Include file name
            SetConsoleTitle(title);
#endif

            const int width = 45;
            int hash_count = (int) (percent * width + 0.5);
            _ftprintf(out, _T("[%.*s%.*s]"), hash_count, _T("############################################################"), width - hash_count, _T("............................................................"));
            _ftprintf(out, _T("%7.2f%% "), percent * 100.0);
            _ftprintf(out, L"\x1b]9;4;1;%d\x1b\\", static_cast<int>(percent * 100 + 0.05));

            DisplaySize(out, Transferred, m_nf);
            _ftprintf(out, _T("/"));
            DisplaySize(out, FileSize, m_nf);
            _ftprintf(out, _T("\r"));
            fflush(out);
        }
        else
        {
            _ftprintf(out, _T("Read: "));
            DisplaySize(out, Transferred, m_nf);
            _ftprintf(out, _T("\r"));
            fflush(out);
        }
    }
private:
    FILE* out;
    const NUMBERFMT* m_nf;
};

DWORD CALLBACK CopyProgress(
  LARGE_INTEGER TotalFileSize,
  LARGE_INTEGER TotalBytesTransferred,
  LARGE_INTEGER /*StreamSize*/,
  LARGE_INTEGER /*StreamBytesTransferred*/,
  DWORD /*dwStreamNumber*/,
  DWORD /*dwCallbackReason*/,
  HANDLE /*hSourceFile*/,
  HANDLE /*hDestinationFile*/,
  LPVOID lpData
)
{
    if (lpData)
        ((DrawProgress*) lpData)->Draw(ToFileSize(TotalBytesTransferred.QuadPart), ToFileSize(TotalFileSize.QuadPart));
    return PROGRESS_CONTINUE;
}

CWinInetHandle& GetInternet()
{
    static CWinInetHandle inet(InternetOpen(TEXT("RadTools"), INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0));
    if (!inet.Get())
        ThrowWinInetError();
    return inet;
}

// TODO This assumes it will be called with the same credentials
// TODO Also doesnt check if it has been lost
CWinInetHandle& GetFtp(const Url& url)
{
    static CWinInetHandle ftp = url.Connect();
    return ftp;
}

class FileReadLocal : public FileRead
{
public:
    FileReadLocal(const TCHAR* file)
    {
        m_file.Open(file, FILE_GENERIC_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL);
    }
    ~FileReadLocal()
    {
    }
    DWORD Read(void *Buffer, const DWORD Size)
    {
        return m_file.Read(Buffer, Size);
    }
    ULARGE_INTEGER GetSize() const
    {
        return m_file.GetSize();
    }
private:
    CWinFile m_file;
};

class FileWriteLocal : public FileWrite
{
public:
    FileWriteLocal(const TCHAR* file, DWORD Attributes)
    {
        // TODO Always hidden??
        m_file.Open(file, FILE_GENERIC_WRITE, 0, OPEN_ALWAYS, Attributes);
    }
    ~FileWriteLocal()
    {
    }
    DWORD Write(void const *Buffer, const DWORD Size)
    {
        return m_file.Write(Buffer, Size);
    }
private:
    CWinFile m_file;
};

class FileReadFtp : public FileRead
{
public:
    FileReadFtp(const Url& url)
    {
        m_file.Open(GetFtp(url).Get(), url.GetPath(), GENERIC_READ, FTP_TRANSFER_TYPE_BINARY, 0);
    }
    ~FileReadFtp()
    {
    }
    DWORD Read(void *Buffer, const DWORD Size)
    {
        return m_file.Read(Buffer, Size);
    }
    ULARGE_INTEGER GetSize() const
    {
        return m_file.GetSize();
    }
private:
    CWinInetFtpFile m_file;
};

class FileWriteFtp : public FileWrite
{
public:
    FileWriteFtp(const Url& url)
    {
        m_file.Open(GetFtp(url).Get(), url.GetPath(), GENERIC_WRITE, FTP_TRANSFER_TYPE_BINARY, 0);
        if (!m_file.Get())
            ThrowWinInetError();
    }
    ~FileWriteFtp()
    {
    }
    DWORD Write(void const *Buffer, const DWORD Size)
    {
        return m_file.Write(Buffer, Size);
    }
private:
    CWinInetFtpFile m_file;
};

class FileReadHttp : public FileRead
{
public:
    FileReadHttp(const Url& url)
    {
        m_file.Open(GetInternet().Get(), url.GetUrl(), NULL, 0, INTERNET_FLAG_PASSIVE, 0);
        if (!m_file.Get())
            ThrowWinInetError();
    }
    ~FileReadHttp()
    {
    }
    DWORD Read(void *Buffer, const DWORD Size)
    {
        return m_file.Read(Buffer, Size);
    }
    ULARGE_INTEGER GetSize() const
    {
        CWinInetFile::FileSizeT size = {};
        m_file.GetSize(&size);
        return size;
    }
private:
    CWinInetHttpFile m_file;
};

class FileWriteHttp : public FileWrite
{
public:
    FileWriteHttp(const Url& url, int buffer_size)
    {
        m_connect = url.Connect();
        m_file.Attach(HttpOpenRequest(m_connect.Get(), TEXT("POST"), url.GetPath(), NULL, NULL, NULL, INTERNET_FLAG_NO_CACHE_WRITE , 0));
        if (!m_file.Get())
            ThrowWinInetError();
        // TODO Setup headers
        INTERNET_BUFFERS BufferIn;
        ZeroMemory(&BufferIn, sizeof(BufferIn));
        BufferIn.dwStructSize = sizeof(INTERNET_BUFFERS);
        BufferIn.dwBufferTotal = buffer_size;
        if (HttpSendRequestEx(m_file.Get(), &BufferIn, NULL, 0, 0) != 0 && GetLastError() != ERROR_SUCCESS)
            ThrowWinInetError();
    }
    ~FileWriteHttp()
    {
        if (HttpEndRequest(m_file.Get(), NULL, 0, 0) && GetLastError() != ERROR_SUCCESS)
            ThrowWinInetError();
    }
    DWORD Write(void const *Buffer, const DWORD Size)
    {
        return m_file.Write(Buffer, Size);
    }
private:
    CWinInetHandle m_connect;
    CWinInetHttpFile m_file;
};

void CopyFile(CWinInetFile& IFile, CWinFile& OFile, CWinInetFile::FileSizeT FileSize, const NUMBERFMT* nf, bool Quiet)
{
    const int BufferSize = 1024 * 10;
    TCHAR *Buffer = new TCHAR[BufferSize];
    ULARGE_INTEGER TotalRead = {};
    DWORD Read = 0;
    DWORD Write = 0;
    DrawProgress dp(nf);
    bool Cont = true;
    while (Cont)
    {
        Read = IFile.Read(Buffer, BufferSize);
        if (Read == 0)
            break;
        TotalRead.QuadPart += Read;
        Write = OFile.Write(Buffer, Read);

        if (Read != Write)
        {
            _tprintf(_T("Error: Read != Write\n")); // TODO Throw?
            Cont = false;
            break;
        }

        if (!Quiet) dp.Draw(TotalRead, FileSize);
    }
    delete [] Buffer;
}

void CopyFile(const Url& source, const Url& destination, const CDirectory::CEntry& entry, const NUMBERFMT* nf)
{
    Url sourcefile = source + entry.GetFileName();
    Url destinationfile = destination + entry.GetFileName();

    // TODO Handle when the copy fails
    // ie in network/ftp transfers
    // should remove the entry from the destination entries list
    // and delete the file
    // that way, when run again, we think it is new at the server
    if (destinationfile.IsLocal())
        destinationfile.RemoveAttr(FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_HIDDEN);

    if (destinationfile.IsLocal())
    {
        int trycount = 0;
        while (trycount >= 0)
        {
            try
            {
                sourcefile.GetLocally(destinationfile.GetUrl(), nf);
                trycount = -1;
            }
            catch (const rad::WinError& e)
            {
                if (trycount > 0)
                    throw;
                else if (e.GetError() == ERROR_SHARING_VIOLATION)
                {
                    ++trycount;
                    std::tstring newDest = destinationfile.GetUrl();
                    newDest += TEXT(".rsp!");
                    if (MoveFileEx(destinationfile.GetUrl(), newDest.c_str(), MOVEFILE_REPLACE_EXISTING) == 0)
                        throw;
                    DWORD Attr = GetFileAttributes(newDest.c_str());
                    if (Attr == INVALID_FILE_ATTRIBUTES)
                        throw;
                    Attr |= FILE_ATTRIBUTE_HIDDEN;
                    SetFileAttributes(newDest.c_str(), Attr);
                }
                else
                    throw;
            }
        }
    }
    else if (sourcefile.IsLocal())
        destinationfile.PutLocally(sourcefile.GetUrl(), nf);
    else
    {
        std::auto_ptr<FileRead> file_read(sourcefile.OpenFileRead());
        std::auto_ptr<FileWrite> file_write(sourcefile.OpenFileWrite(1024, false));

        DrawProgress dp(nf);

        char ReadBuffer[1024];
        ULARGE_INTEGER Transferred = ToFileSize(0);
        DWORD ReadSize;
        while ((ReadSize = file_read->Read(ReadBuffer, 1024)) > 0)
        {
            char* WriteBuffer = ReadBuffer;
            DWORD WriteSize;
            while ((WriteSize = file_write->Write(WriteBuffer, ReadSize)) < ReadSize)
            {
                WriteBuffer += WriteSize;
                ReadSize -= WriteSize;
            }
            Transferred.QuadPart += WriteSize;
            dp.Draw(Transferred, file_read->GetSize());
        }
    }
    //destinationfile.SetTime(&entry.ftCreationTime, &entry.ftLastAccessTime, &entry.ftLastWriteTime);
    destinationfile.SetTime(0, 0, &entry.ftLastWriteTime);
}

void DeleteFile(const Url& destination, const CDirectory::CEntry& entry)
{
    Url destinationfile = destination + entry.GetFileName();
    destinationfile.DeleteFile();
}

void Url::Init(const TCHAR *url)
{
    _tcscpy_s(m_Url, url);
    ZeroMemory(this, sizeof(URL_COMPONENTS));
    m_Scheme[0] = TEXT('\0');
    m_Host[0] = TEXT('\0');
    m_User[0] = TEXT('\0');
    m_Password[0] = TEXT('\0');
    m_Path[0] = TEXT('\0');
    _tcscpy_s(m_Path, url);

    dwStructSize = sizeof(URL_COMPONENTS);
    lpszScheme = m_Scheme;
    dwSchemeLength = 100;
    lpszHostName = m_Host;
    dwHostNameLength = 100;
    lpszUserName = m_User;
    dwUserNameLength = 100;
    lpszPassword = m_Password;
    dwPasswordLength = 100;
    lpszUrlPath = m_Path;
    dwUrlPathLength = 1024;
    dwExtraInfoLength = 0;
    InternetCrackUrl(url, 0, 0, this);

    if (nScheme == INTERNET_SCHEME_DEFAULT && m_Path[0] == TEXT('~'))
    {
        TCHAR home[MAX_PATH];
        if ((GetEnvironmentVariable(TEXT("HOME"), home, ARRAYSIZE(home)) != 0) || (GetEnvironmentVariable(TEXT("USERPROFILE"), home, ARRAYSIZE(home)) != 0))
        {
            const size_t lenPath = _tcslen(m_Path);
            const size_t lenHome = _tcslen(home);
            wmemmove(m_Path + lenHome - 1 + 1, m_Path + 1, lenPath - 1 + 1);
            wmemcpy(m_Path, home, lenHome);
        }
    }
}

void Url::Change(const TCHAR *subdir)
{
    if (lpszUrlPath[0] != TEXT('\0'))
        AppendDelim();
    _tcscat_s(lpszUrlPath, ARRAYSIZE(m_Path), subdir);
    _tcscat_s(m_Url, subdir);
}

void Url::AppendDelim()
{
    TCHAR delim = GetDelim();
    size_t len;
    len = _tcslen(lpszUrlPath);
    if (lpszUrlPath[len - 1] != delim)
    {
        lpszUrlPath[len] = delim;
        lpszUrlPath[len + 1] = TEXT('\0');
    }
    len = _tcslen(m_Url);
    if (m_Url[len - 1] != delim)
    {
        m_Url[len] = delim;
        m_Url[len + 1] = TEXT('\0');
    }
}

void Url::SetTime(const FILETIME* lpCreationTime, const FILETIME* lpLastAccessTime, const FILETIME* lpLastWriteTime) const
{
    switch(nScheme)
    {
    case INTERNET_SCHEME_FTP:
        {
            // MDTM [YYYYMMDDHHMMSS] filename.ext
            SYSTEMTIME stUTC;
            if (FileTimeToSystemTime(lpLastWriteTime, &stUTC) == 0)
                rad::ThrowWinError();
#if 0
            SYSTEMTIME stLocal;
            if (SystemTimeToTzSpecificLocalTime(NULL, &stUTC, &stLocal) == 0)
                rad::ThrowWinError();
#endif

            TCHAR Date[1024];
            /*int LengthD =*/ GetDateFormat(LOCALE_USER_DEFAULT, 0, &stUTC, TEXT("yyyyMMdd"), Date, 1024);
            TCHAR Time[1024];
            /*int LengthT =*/ GetTimeFormat(LOCALE_USER_DEFAULT, 0, &stUTC, TEXT("HHmmss"), Time, 1024);

            TCHAR Command[1024];
            //_stprintf_s(Command, TEXT("MFMT %s%s %s"), Date, Time, GetPath() + 1);
            //_stprintf_s(Command, TEXT("SITE UTIME %s%s %s"), Date, Time, GetPath() + 1);
            _stprintf_s(Command, TEXT("SITE UTIME %s %s%s %s%s %s%s UTC"), GetPath(), Date, Time, Date, Time, Date, Time);
            HINTERNET ftpCommand = NULL;
            if (FtpCommand(GetFtp(*this).Get(), FALSE, FTP_TRANSFER_TYPE_ASCII, Command, 0, &ftpCommand) == 0)
            {
                //ThrowWinInetError();
                _ftprintf(stderr, _T("Error executing FtpCommand: %s\n"), Command);
                rad::WinError error(GetModuleHandle(TEXT("wininet.dll")));
                if (error.GetError() == 12003) // The server returned extended information
                {
                    DWORD e;
                    TCHAR Buffer[1024];
                    DWORD s = 1024;
                    InternetGetLastResponseInfo(&e, Buffer, &s);
                    _ftprintf(stderr, Buffer);
                }
            }
            if (ftpCommand)
            {
                CWinInetHandle ret(ftpCommand);

                TCHAR Response[1024];

                TCHAR* Buffer = Response;
                DWORD Read;
                do
                {
                    if (InternetReadFile(ret.Get(), Buffer, 1024, &Read) == 0)
                        ThrowWinInetError();
                    Buffer += Read;
                } while (Read > 0);
            }
        }
        break;

    case INTERNET_SCHEME_DEFAULT:
    case INTERNET_SCHEME_UNKNOWN:
        {
            CWinFile file;
            file.Open(GetUrl(), FILE_WRITE_ATTRIBUTES, 0, OPEN_EXISTING);
            file.SetTime(lpCreationTime, lpLastAccessTime, lpLastWriteTime);
        }
        break;

    default:
        assert(false);
        break;
    }
}

void Url::DeleteFile() const
{
    switch(nScheme)
    {
    case INTERNET_SCHEME_FTP:
        if (FtpDeleteFile(GetFtp(*this).Get(), GetPath()) == 0)
            ThrowWinInetError();
        break;

    case INTERNET_SCHEME_DEFAULT:
    case INTERNET_SCHEME_UNKNOWN:
        RemoveAttr(FILE_ATTRIBUTE_READONLY);
        if (::DeleteFile(GetUrl()) == 0)
            rad::ThrowWinError(TEXT("DeleteFile : "));
        break;

    default:
        assert(false);
        break;
    }
}

void Url::CreateDirectory() const
{
    switch(nScheme)
    {
    case INTERNET_SCHEME_FTP:
        if (FtpCreateDirectory(GetFtp(*this).Get(), GetPath()) == 0)
            ThrowWinInetError();
        break;

    case INTERNET_SCHEME_DEFAULT:
    case INTERNET_SCHEME_UNKNOWN:
        if (::CreateDirectory(GetUrl(), 0) == 0)
            rad::ThrowWinError(TEXT("CreateDirectory : "));
        break;

    default:
        assert(false);
        break;
    }
}

void Url::DeleteDirectory() const
{
    switch(nScheme)
    {
    case INTERNET_SCHEME_FTP:
        if (FtpRemoveDirectory(GetFtp(*this).Get(), GetPath()) == 0)
            ThrowWinInetError();
        break;

    case INTERNET_SCHEME_DEFAULT:
    case INTERNET_SCHEME_UNKNOWN:
        if (::RemoveDirectory(GetUrl()) == 0)
            rad::ThrowWinError(TEXT("DeleteDirectory : "));
        break;

    default:
        assert(false);
        break;
    }
}

void Url::RemoveAttr(DWORD RemAttr) const
{
    assert(nScheme != INTERNET_SCHEME_FTP); // TODO Fix this for ftp

    DWORD Attr = GetFileAttributes(GetUrl());
    if (Attr != INVALID_FILE_ATTRIBUTES && Attr & RemAttr)
    {
        Attr &= ~RemAttr;
        if (SetFileAttributes(GetUrl(), Attr) == 0)
            rad::ThrowWinError(TEXT("SetFileAttributes : "));
    }
}

CWinInetHandle Url::Connect() const
{
    LPTSTR  sUserName = lpszUserName;
    std::tstring  sPassword = lpszPassword;
    PCREDENTIAL pCred = nullptr;
    if (lpszUserName[0] == _T('\0') && CredRead((GetScheme() + std::tstring(_T("://")) + GetHost()).c_str(), CRED_TYPE_GENERIC, 0, &pCred))
    {
        sUserName = pCred->UserName;
        sPassword = std::tstring((LPTSTR) pCred->CredentialBlob, pCred->CredentialBlobSize / sizeof(TCHAR));
    }

    DWORD dwService = 0;
    DWORD dwFlags = 0;
    switch (nScheme)
    {
    case INTERNET_SCHEME_FTP:
        dwService = INTERNET_SERVICE_FTP;
        dwFlags = INTERNET_FLAG_PASSIVE;
        break;

    case INTERNET_SCHEME_HTTP:
    case INTERNET_SCHEME_HTTPS:
        dwService = INTERNET_SERVICE_HTTP;
        break;
    }

    CWinInetHandle ftp = InternetConnect(GetInternet().Get(), GetHost(), GetPort(), sUserName, sPassword.c_str(), dwService, dwFlags, 0);
    SecureZeroMemory((PVOID) sPassword.data(), sPassword.size() * sizeof(TCHAR));

    if (pCred != nullptr)
        CredFree(pCred);

    if (!ftp.Get())
        ThrowWinInetError();

    return ftp;
}

std::auto_ptr<FileRead> Url::OpenFileRead() const
{
    switch(nScheme)
    {
    case INTERNET_SCHEME_FTP:
        return std::auto_ptr<FileRead>(new FileReadFtp(*this));
        break;

    case INTERNET_SCHEME_HTTP:
    case INTERNET_SCHEME_HTTPS:
        return std::auto_ptr<FileRead>(new FileReadHttp(*this));
        break;

    case INTERNET_SCHEME_DEFAULT:
    case INTERNET_SCHEME_UNKNOWN:
        return std::auto_ptr<FileRead>(new FileReadLocal(GetUrl()));
        break;

    default:
        return std::auto_ptr<FileRead>();
        break;
    }
}

std::auto_ptr<FileWrite> Url::OpenFileWrite(int buffer_size, bool Hidden) const
{
    switch(nScheme)
    {
    case INTERNET_SCHEME_FTP:
        return std::auto_ptr<FileWrite>(new FileWriteFtp(*this));
        break;

    case INTERNET_SCHEME_HTTP:
    case INTERNET_SCHEME_HTTPS:
        return std::auto_ptr<FileWrite>(new FileWriteHttp(*this, buffer_size));
        break;

    case INTERNET_SCHEME_DEFAULT:
    case INTERNET_SCHEME_UNKNOWN:
        return std::auto_ptr<FileWrite>(new FileWriteLocal(GetUrl(), Hidden ? FILE_ATTRIBUTE_NORMAL | FILE_ATTRIBUTE_HIDDEN : FILE_ATTRIBUTE_NORMAL));
        break;

    default:
        return std::auto_ptr<FileWrite>();
        break;
    }
}

bool Url::GetLocally(const TCHAR* destination, const NUMBERFMT* nf) const
{
    switch(nScheme)
    {
    case INTERNET_SCHEME_FTP:
#if 0
        // TODO Use CopyFile so we can have a progress meter
        if (FtpGetFile(GetFtp(*this).Get(), GetPath(), destination, FALSE, FILE_ATTRIBUTE_NORMAL, FTP_TRANSFER_TYPE_UNKNOWN, 0) == 0)
            ThrowWinInetError();
#else
        {
            CWinInetFtpFile IFile;
            IFile.Open(GetFtp(*this).Get(), GetPath(), GENERIC_READ, FILE_ATTRIBUTE_NORMAL, 0);

            CWinInetFile::FileSizeT FileSize = IFile.GetSize();

            CWinFile OFile;
            OFile.Open(destination, FILE_WRITE_DATA | FILE_WRITE_ATTRIBUTES, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL);
            CopyFile(IFile, OFile, FileSize, nf, false);
        }
#endif
        break;

    case INTERNET_SCHEME_HTTP:
    case INTERNET_SCHEME_HTTPS:
        {
            CWinInetHttpFile IFile;
            DWORD Flags = INTERNET_FLAG_PASSIVE;
            IFile.Open(GetInternet().Get(), GetUrl(), NULL, 0, Flags, 0);

            CWinFile OFile;
            OFile.Open(destination, FILE_WRITE_DATA | FILE_WRITE_ATTRIBUTES, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL);

            CWinInetFile::FileSizeT FileSize;
            IFile.GetSize(&FileSize);

            CopyFile(IFile, OFile, FileSize, nf, false);
        }
        break;

    case INTERNET_SCHEME_DEFAULT:
    case INTERNET_SCHEME_UNKNOWN:
        {
            DrawProgress    dp(nf);
            if (CopyFileEx(GetUrl(), destination, CopyProgress, &dp, NULL, COPY_FILE_ALLOW_DECRYPTED_DESTINATION) == 0)
                rad::ThrowWinError(TEXT("CopyFile : "));
        }
        break;

    default:
        //assert(false);
        return false;
        break;
    }
    return true;
}

bool Url::PutLocally(const TCHAR* source, const NUMBERFMT* nf) const
{
    switch(nScheme)
    {
    case INTERNET_SCHEME_FTP:
        // TODO What to do when the path is relative to the initial path
        //if (FtpPutFile(GetFtp(*this).Get(), source, GetPath() + 1, FTP_TRANSFER_TYPE_UNKNOWN, 0) == 0)
        // TODO Use CopyFile so we can have a progress meter
        if (FtpPutFile(GetFtp(*this).Get(), source, GetPath(), FTP_TRANSFER_TYPE_UNKNOWN, 0) == 0)
            ThrowWinInetError();
        break;

    case INTERNET_SCHEME_DEFAULT:
    case INTERNET_SCHEME_UNKNOWN:
        {
            DrawProgress    dp(nf);
            if (CopyFileEx(source, GetUrl(), CopyProgress, &dp, NULL, COPY_FILE_ALLOW_DECRYPTED_DESTINATION) == 0)
                rad::ThrowWinError(TEXT("CopyFile : "));
        }
        break;

    default:
        //assert(false);
        return false;
        break;
    }
    return true;
}

void GetDirectory(const Url& url, std::vector<CDirectory::CEntry>& dirlist, bool ShowHidden)
{
    dirlist.clear();
    if (url.nScheme == INTERNET_SCHEME_FTP)
    {
        SetLastError(0);

        CWinInetHandle& ftp = GetFtp(url);

        TCHAR TempPath[MAX_PATH];
        _tcscpy_s(TempPath, url.lpszUrlPath);
        TCHAR* Search = _tcsrchr(TempPath, TEXT('/'));
        if (Search)
        {
            *Search = TEXT('\0');
            ++Search;
        }

        if (TempPath[0])
            if (!FtpSetCurrentDirectory(ftp.Get(), TempPath))
                ThrowWinInetError();

        CDirectory::CEntry  dir_entry;
        CWinInetHandle find = FtpFindFirstFile(ftp.Get(), Search, &dir_entry, 0, 0);
        // NOTE dir_entry.ftLastWriteTime /etc are in the local time zone
        if (find.Get() != NULL)
        {
            do
            {
                if (dir_entry.cFileName[0] == TEXT('.'))
                {
                    dir_entry.dwFileAttributes |= FILE_ATTRIBUTE_HIDDEN;
                }
                dirlist.push_back(dir_entry);
            } while (InternetFindNextFile(find.Get(), &dir_entry));
        }
        else
        {
            rad::WinError error(GetModuleHandle(TEXT("wininet.dll")), TEXT("Error opening directory ") + std::tstring(url.lpszUrlPath) + TEXT(" : "));
            if (error.GetError() != ERROR_NO_MORE_FILES)
                ThrowWinError(error);
        }
    }
    else
    {
        std::tstring search( url.GetPath() );
        if (search.empty())
            search += TEXT("*.*");
        else if (*search.rbegin() == TEXT('.'))
        {       // This assumes names ending in '.' are directories which isn't always true
            CDirectory::AppendDelim(search);
            search += TEXT("*.*");
        }
        else if ((search.find_first_of(TEXT("*?")) == std::tstring::npos) && CDirectory::Exists(search))
        {
            CDirectory::AppendDelim(search);
            search += TEXT("*.*");
        }
        else if (*search.rbegin() == TEXT(':') || *search.rbegin() == CDirectory::GetDelim())
        {
            search += TEXT("*.*");
        }

        CDirectory::CEntry  dir_entry;
        CDirectory          dir(search, dir_entry);
        if (dir.IsValid())
        {
            if (ShowHidden)
            {
                do
                {
                    dirlist.push_back(dir_entry);
                } while (dir.NextFile(dir_entry));
            }
            else
            {
                do
                {
                    if (!dir_entry.IsHidden())
                        dirlist.push_back(dir_entry);
                } while (dir.NextFile(dir_entry));
            }
        }
        else
        {
            rad::WinError error(TEXT("Error opening directory ") + std::tstring(url.GetUrl()) + TEXT(" : "));
            if (error.GetError() != ERROR_FILE_NOT_FOUND)
                ThrowWinError(error);
        }
    }
}

bool DirSorter::operator()(const CDirectory::CEntry& left, const CDirectory::CEntry& right) const
{
    if (m_DirFirst && left.IsDirectory() && right.IsDirectory())
        return Compare(left, right);
    else if (m_DirFirst && left.IsDirectory())
        return true;
    else if (m_DirFirst && right.IsDirectory())
        return false;
    else
        return Compare(left, right);
}

bool DirSorter::Compare(const CDirectory::CEntry& left, const CDirectory::CEntry& right) const
{
    switch(m_Order)
    {
    default:
    case Order::Name:
        return CompareName(left, right);
        break;

    case Order::Size:
        return CompareSize(left, right);
        break;

    case Order::Date:
        return CompareDate(left, right);
        break;
    }
}

bool DirSorter::CompareName(const CDirectory::CEntry& left, const CDirectory::CEntry& right)
{
    return _tcsicmp(left.GetFileName(), right.GetFileName()) < 0;
}

bool DirSorter::CompareSize(const CDirectory::CEntry& left, const CDirectory::CEntry& right)
{
    if (left.nFileSizeHigh == right.nFileSizeHigh)
        return left.nFileSizeLow < right.nFileSizeLow;
    else
        return left.nFileSizeHigh < right.nFileSizeHigh;
}

bool DirSorter::CompareDate(const CDirectory::CEntry& left, const CDirectory::CEntry& right)
{
    return left.ftLastWriteTime < right.ftLastWriteTime;
}

void SortDirectory(std::vector<CDirectory::CEntry>& dirlist, const DirSorter& sorter)
{
    sort(dirlist.begin(), dirlist.end(), sorter);
}
