//#include <TChar.H>

#include <Rad/Win/WinFile.h>
#include <Rad/Win/WinInetFile.h>
#include <Rad/ConsoleUtils.H>
#include <Rad/Directory.H>
#include <Rad/DirHelper.H>
#include <Rad/NumberFormat.h>
#include <Rad/About/AboutMessage.h>

// TODO
// If output is a directory append name
// Taskbar progress
// Set headers, specifically cookies
// Output to stdout
// If name is empty set to index.html

enum RetCode
{
    RET_OK,
    RET_WINDOWS_EXCEPTION,
    RET_UNKNOWN_EXCEPTION,
    RET_UNKNOWN_SCHEME,
    RET_BAD_REQUEST,
    RET_SKIP_FILE,
};

bool GetUrl(CWinInetFile& IFile, TCHAR* Url, DWORD dwSize)
{
    DWORD Size;
    return InternetQueryOption(IFile.Get(), INTERNET_OPTION_URL, Url, &(Size = dwSize)) != FALSE
        && InternetCanonicalizeUrl(Url, Url, &(Size = 1024), ICU_DECODE | ICU_NO_ENCODE) != FALSE;
}

RetCode HttpDownload(const CWinInetHandle& inet, const TCHAR* InputFile, const TCHAR* OutputFile, DWORD& StatusCode, bool ShowHeaders, bool Reload, bool CheckNewer, const NUMBERFMT* nf)
{
    CWinInetHttpFile IFile;
    DWORD Flags = 0;
    if (Reload)
        Flags |= INTERNET_FLAG_RELOAD;
    IFile.Open(inet.Get(), InputFile, NULL, 0, Flags, 0);

    StatusCode = IFile.GetStatusCode();

    SYSTEMTIME SysFileDate;
    bool SysFileDateValid = IFile.GetLastModified(&SysFileDate);
    CWinInetFile::FileSizeT FileSize;
    if (!IFile.GetSize(&FileSize))
        FileSize.QuadPart = INT64_MAX;

    TCHAR ContentDisposition[1024] = TEXT("");
    {
        if (!OutputFile && IFile.GetContentDisposition(ContentDisposition, 1024))
        {
            _tprintf(_T("ContentDisposition: %s\n"), ContentDisposition);
            OutputFile = _tcsstr(ContentDisposition, TEXT("filename="));
            if (OutputFile)
            {
                if (OutputFile)
                    OutputFile += _tcslen(TEXT("filename="));
                const TCHAR* OutputFileEnd = nullptr;
                if (*OutputFile == '"')
                {
                    ++OutputFile;
                    OutputFileEnd = _tcschr(OutputFile, TEXT('"'));
                }
                else
                    OutputFileEnd = _tcschr(OutputFile, TEXT(' '));
                if (OutputFileEnd)
                    ContentDisposition[OutputFileEnd - ContentDisposition] = '\0';
            }
        }
    }

    TCHAR Url[1024] = TEXT("");
    {
        if (!OutputFile && GetUrl(IFile, Url, 1024))
        {
            _tprintf(_T("Url: %s\n"), Url);
            OutputFile = _tcsrchr(Url, TEXT('/'));
            if (OutputFile)
                ++OutputFile;
            const TCHAR* OutputFileEnd = _tcsrchr(OutputFile, TEXT('?'));
            if (OutputFileEnd)
                Url[OutputFileEnd - Url] = '\0';
        }
    }

    if (StatusCode != HTTP_STATUS_OK)
        _tprintf(_T("Status: %u\n"), StatusCode);
    if (ShowHeaders)
    {
        TCHAR Headers[4096] = TEXT("");
        if (IFile.GetHeaders(Headers, 4095))
            _tprintf(_T("Headers: %s\n"), Headers);
        else
        {
            DWORD e = GetLastError();
            _tprintf(_T("Headers: Error %u\n"), e);
        }
    }
    _tprintf(_T("Output: %s\n"), OutputFile);
    {
        TCHAR ContentType[1024] = TEXT("");
        IFile.GetContentType(ContentType, 1023);
        _tprintf(_T("ContentType: %s\n"), ContentType);
    }
    _tprintf(_T("Size: "));
    if (FileSize.QuadPart != INT64_MAX)
        DisplaySize(stdout, FileSize, nf);
    else
        _tprintf(_T("Unknown"));
    _tprintf(_T("\n"));

    if (StatusCode < HTTP_STATUS_BAD_REQUEST)
    {
        bool Skip = false;
        if (CheckNewer && SysFileDateValid)
        {
            CWinFile OFile;
            OFile.Open(OutputFile, FILE_READ_DATA | FILE_READ_ATTRIBUTES, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL);
            if (OFile.IsOpen())
            {
                FILETIME FileFileDate = { 0 , 0 };
                FILETIME UrlFileDate = { 0 , 0 };

                OFile.GetTime(NULL, NULL, &FileFileDate);
                if (SystemTimeToFileTime(&SysFileDate, &UrlFileDate) == 0)
                    rad::ThrowWinError(TEXT("SystemTimeToFileTime : "));

                if (FileFileDate > UrlFileDate)
                    Skip = true;
            }
        }

        if (!Skip)
        {
            CWinFile OFile;
            const DWORD Share = _tcscmp(OutputFile, TEXT("CONOUT$")) == 0 ? FILE_SHARE_WRITE : 0;
            OFile.Open(OutputFile, GENERIC_WRITE | FILE_WRITE_DATA | FILE_WRITE_ATTRIBUTES, Share, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL);
            CopyFile(IFile, OFile, FileSize, nf);

            if (SysFileDateValid && OFile.GetType() == FILE_TYPE_DISK)
            {
                FILETIME FileFileDate = { 0 , 0 };
                if (SystemTimeToFileTime(&SysFileDate, &FileFileDate) == 0)
                    rad::ThrowWinError(TEXT("SystemTimeToFileTime : "));
                //if (LocalFileTimeToFileTime(&FileFileDate, &FileFileDate) == 0)
                    //rad::ThrowWinError(TEXT("LocalFileTimeToFileTime : "));
                OFile.SetTime(NULL, NULL, &FileFileDate);
            }

            return RET_OK;
        }
        else
        {
            _tprintf(_T("Skipping, file is newer."));
            return RET_SKIP_FILE;
        }
    }
    else
        return RET_BAD_REQUEST;
}

RetCode FtpDownload(const CWinInetHandle& inet, const TCHAR* Host, INTERNET_PORT nPort, const TCHAR* User, const TCHAR* Password, const TCHAR* Path, const TCHAR* InputFile, const TCHAR* OutputFile, bool Reload, const NUMBERFMT* nf)
{
    DWORD Flags = INTERNET_FLAG_PASSIVE;
    if (Reload)
        Flags |= INTERNET_FLAG_RELOAD;
    CWinInetHandle ftp(InternetConnect(inet.Get(), Host, nPort, User, Password, INTERNET_SERVICE_FTP, Flags, 0));
    if (!ftp.Get())
        ThrowWinInetError();

    CDirectory::CEntry  dir_entry;
    {
        CWinInetHandle find = FtpFindFirstFile(ftp.Get(), Path, &dir_entry, 0, 0);
        if (find.Get() != NULL)
        {
            do
            {
                //OFile.SetTime(NULL, NULL, &dir_entry.ftLastWriteTime);
                //dirlist.push_back(dir_entry);
            } while (InternetFindNextFile(find.Get(), &dir_entry));
        }
        else
        {
            ThrowWinInetError(TEXT("Error opening directory: "));
        }
    }

    if (dir_entry.IsDirectory())
    {
        _ftprintf(stderr, _T("%s is a directory\n"), InputFile);
    }
    else
    {
        CWinInetFtpFile IFile;
        IFile.Open(ftp.Get(), Path, GENERIC_READ, FTP_TRANSFER_TYPE_BINARY, 0);

        if (!OutputFile)
        {
            OutputFile = dir_entry.GetFileName();
        }

        CWinInetFile::FileSizeT FileSize = IFile.GetSize();

        _tprintf(_T("Output: %s\n"), OutputFile);
        _tprintf(_T("Size: "));
        DisplaySize(stdout, FileSize, nf);
        _tprintf(_T("\n"));

        CWinFile OFile;
        const DWORD Share = _tcscmp(OutputFile, TEXT("CONOUT$")) == 0 ? FILE_SHARE_WRITE : 0;
        OFile.Open(OutputFile, GENERIC_WRITE | FILE_WRITE_DATA | FILE_WRITE_ATTRIBUTES, Share, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL);
        CopyFile(IFile, OFile, FileSize, nf);

        OFile.SetTime(NULL, NULL, &dir_entry.ftLastWriteTime);
    }

    return RET_OK;
}

int tmain(int argc, TCHAR *argv[])
{
    RetCode r = RET_OK;
    DWORD StatusCode = HTTP_STATUS_OK;

    try
    {
        const TCHAR* InputFile = 0;
        const TCHAR* OutputFile = 0;
        bool UseHttp = false;
        bool ShowHeaders = false;
        bool Reload = false;
        bool CheckNewer = false;
        bool ShowUsage = false;

        NumberFormat    nf(LOCALE_USER_DEFAULT);
        nf.NumDigits = 0;

        for (int i = 1; i < argc; ++i)
        {
            if (argv[i][0] == _T('-') || argv[i][0] == _T('/'))
            {
                if (_tcscmp(argv[i] + 1, TEXT("http")) == 0)
                    UseHttp = true;
                else if (_tcscmp(argv[i] + 1, TEXT("headers")) == 0)
                    ShowHeaders = true;
                else if (_tcscmp(argv[i] + 1, TEXT("reload")) == 0)
                    Reload = true;
                else if (_tcscmp(argv[i] + 1, TEXT("newer")) == 0)
                    CheckNewer = true;
                else if (_tcscmp(argv[i] + 1, TEXT("?")) == 0)
                    ShowUsage = true;
                else
                    _ftprintf(stderr, _T("Error: Unknown option -  %s\n"), argv[i]);
            }
            else if (InputFile == 0)
                InputFile = argv[i];
            else if (OutputFile == 0)
                OutputFile = argv[i];
            else
                _ftprintf(stderr, _T("Error: Too many options -  %s\n"), argv[i]);
        }

        //if (!UseThousandSep)
            //nf.Grouping = 0;

#ifdef _DEBUG
        //InputFile = TEXT("http://download.mozilla.org/?product=firefox-1.5.0.6&os=win&lang=en-US");
        //InputFile = TEXT("ftp://adam:xxx@lion.preston.net/staff/adam/tms_devel");
        //InputFile = TEXT("ftp://ftp.microsoft.com/ResKit/win2000/drivers.zip");
        //InputFile = TEXT("ftp://adam:xxx@lion.preston.net/staff/adam/a");
        //UseHttp = true;
#endif
        if (!ShowUsage && InputFile)
        {
            _tprintf(_T("Input: %s\n"), InputFile);
            //_tprintf(_T("Output: %s\n"), OutputFile);

            CWinInetHandle inet(InternetOpen(TEXT("RadGetUrl"), INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0));
            //CWinInetHandle inet(InternetOpen(TEXT("RadGetUrl"), INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0));
            if (!inet.Get())
                ThrowWinInetError();

            TCHAR Host[100];
            TCHAR User[100];
            TCHAR Password[100];
            TCHAR Path[1024];

            Url  UrlInput(InputFile);

            switch (UrlInput.nScheme)
            {
            case INTERNET_SCHEME_HTTP:
            case INTERNET_SCHEME_HTTPS:
                r = HttpDownload(inet, InputFile, OutputFile, StatusCode, ShowHeaders, Reload, CheckNewer, &nf);
                break;

            case INTERNET_SCHEME_FTP:
                if (UseHttp)
                    r = HttpDownload(inet, InputFile, OutputFile, StatusCode, ShowHeaders, Reload, CheckNewer, &nf);
                else
                    // TODO Cant show headers
                    // TODO Use CheckNewer
                    r = FtpDownload(inet, Host, UrlInput.nPort, User, Password, Path, InputFile, OutputFile, Reload, &nf);
                break;

            default:
                r = RET_UNKNOWN_SCHEME;
                break;
            }
        }
        else
        {
            HMODULE    Module = GetModuleHandle(NULL);
            DisplayAboutMessage(Module, TEXT("RadGetUrl"));

            const HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
            const WORD OriginalAttribute = GetConsoleTextAttribute(hOut);
            const WORD HiliteAttribute = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;

            _tprintf(TEXT("\nDownload a file from a url.\n\nRadGetUrl <options> [InputFile] <OutputFile>\n\nOptions:\n"));
            TCHAR* options[][2] = {
                { TEXT("http   "),  TEXT("Force http download") },
                { TEXT("headers"),  TEXT("Show headers (http only)") },
                { TEXT("reload "),  TEXT("Force a reload") },
                { TEXT("newer  "),  TEXT("Check if newer than local (http only)") },
            };
            for (int i = 0; i < ARRAYSIZE(options); ++i)
            {
                SetConsoleTextAttribute(hOut, HiliteAttribute);
                _tprintf(_T("    -%s"), options[i][0]);
                SetConsoleTextAttribute(hOut, OriginalAttribute);
                _tprintf(_T("  %s\n"), options[i][1]);
            }
            SetConsoleTextAttribute(hOut, OriginalAttribute);
        }
    }
    catch(const rad::WinError &e)
    {
        _ftprintf(stderr, e.GetString().c_str());
        return RET_WINDOWS_EXCEPTION;
    }
    catch(...)
    {
        _ftprintf(stderr, TEXT("Unknown Exception\n"));
        return RET_UNKNOWN_EXCEPTION;
    }

    if (StatusCode != HTTP_STATUS_OK)
        return StatusCode;
    else
        return r;
}
