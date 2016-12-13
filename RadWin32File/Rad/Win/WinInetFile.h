#ifndef WININETFILE_H
#define WININETFILE_H

#include "WinInetHandle.H"

class CWinInetFile
{
public:
    typedef    ULARGE_INTEGER   FileSizeT;
    typedef    DWORD    FileRWT;

    void Open(HINTERNET hInternet, const TCHAR* lpszUrl, const TCHAR* lpszHeaders, DWORD dwHeadersLength, DWORD dwFlags, DWORD_PTR dwContext)
    {
        Attach(InternetOpenUrl(hInternet, lpszUrl, lpszHeaders, dwHeadersLength, dwFlags, dwContext));
        if (!IsOpen())
            ThrowWinInetError(TEXT("InternetOpenUrl : "));
    }

    HINTERNET Get() const
    {
        return m_File.Get();
    }

    bool IsOpen() const
    {
        return m_File.Get() != 0;
    }

    void Close()
    {
        m_File.Close();
    }

    FileRWT Read(void *Buffer, const FileRWT Size) const
    {
        FileRWT    BytesRead = 0;
        if (InternetReadFile(m_File.Get(), Buffer, Size, &BytesRead) == 0)
            ThrowWinInetError(TEXT("InternetReadFile : "));
        return BytesRead;
    }

    FileRWT Write(void const *Buffer, const FileRWT Size) const
    {
        FileRWT    BytesWritten = 0;
        if (InternetWriteFile(m_File.Get(), Buffer, Size, &BytesWritten) == 0)
            ThrowWinInetError(TEXT("InternetWriteFile : "));
        return BytesWritten;
    }

//protected:
    void Attach(HINTERNET hInternet)
    {
        //assert(!IsOpen());
        m_File.Attach(hInternet);
    }

private:
    CWinInetHandle    m_File;
};

class CWinInetHttpFile : public CWinInetFile
{
public:
    bool GetSize(FileSizeT* oFileSize) const
    {
        DWORD FileSize;
        DWORD Size = sizeof(FileSize);
        DWORD Index = 0;
        bool result = HttpQueryInfo(Get(), HTTP_QUERY_FLAG_NUMBER | HTTP_QUERY_CONTENT_LENGTH, &FileSize, &Size, &Index) != FALSE;
        oFileSize->QuadPart = FileSize;
        return result;
    }

    bool GetLastModified(SYSTEMTIME* SysFileDate) const
    {
        DWORD Size = sizeof(*SysFileDate);
        DWORD Index = 0;
        return HttpQueryInfo(Get(), HTTP_QUERY_FLAG_SYSTEMTIME | HTTP_QUERY_LAST_MODIFIED, SysFileDate, &Size, &Index) != FALSE;
    }

    bool GetInfoString(DWORD dwInfoLevel, TCHAR* Buffer, DWORD SizeIn) const
    {
        DWORD Index = 0;
        DWORD Size = SizeIn;
        return HttpQueryInfo(Get(), dwInfoLevel, Buffer, &Size, &Index) != FALSE;
    }

    bool GetHeaders(TCHAR* Headers, DWORD SizeIn) const
    {
        return GetInfoString(HTTP_QUERY_RAW_HEADERS_CRLF, Headers, SizeIn);
    }

    bool GetContentType(TCHAR* ContentType, DWORD SizeIn) const
    {
        return GetInfoString(HTTP_QUERY_CONTENT_TYPE, ContentType, SizeIn);
    }

    bool GetContentDisposition(TCHAR* ContentDisposition, DWORD SizeIn) const
    {
        return GetInfoString(HTTP_QUERY_CONTENT_DISPOSITION, ContentDisposition, SizeIn);
    }

    DWORD GetStatusCode() const
    {
        DWORD StatusCode = 0;
        DWORD Size = sizeof(StatusCode);
        if (HttpQueryInfo(Get(), HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &StatusCode, &Size, NULL) == FALSE)
            ThrowWinInetError(TEXT("GetStatusCode : "));
        return StatusCode;
    }
};

class CWinInetFtpFile : public CWinInetFile
{
public:
    void Open(HINTERNET hInternet, const TCHAR* FileName, DWORD Access, DWORD dwFlags, DWORD_PTR dwContext)
    {
        Attach(FtpOpenFile(hInternet, FileName, Access, dwFlags, dwContext));
        if (!IsOpen())
            ThrowWinInetError(TEXT("FtpOpenFile : "));
    }

    FileSizeT GetSize() const   // Only works if handle opened using FtpOpenFile
    {
        FileSizeT    Size;

        Size.LowPart = FtpGetFileSize(Get(), &Size.HighPart);

        if (Size.LowPart == 0xFFFFFFFF)
            ThrowWinInetError(TEXT("GetSize : "));

        return Size;
    }
};

#endif
