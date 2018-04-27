#ifndef WINFILE_H
#define WINFILE_H

#include <Rad/Win/WinHandle.h>

class CWinFile
{
public:
    typedef	ULARGE_INTEGER  FileSizeT;
    typedef	DWORD   FileRWT;

    void Open(const TCHAR *FileName, DWORD Access, DWORD Share, DWORD CreationDisposition, DWORD Attributes = FILE_ATTRIBUTE_NORMAL)
    {
        m_File = rad::WinHandle(CreateFile(FileName, Access, Share, NULL, CreationDisposition, Attributes, NULL));
        if (CreationDisposition != OPEN_EXISTING && !IsOpen())
            rad::ThrowWinError(TEXT("CreateFile : "));
    }

    bool IsOpen() const
    {
        return m_File.Get() != INVALID_HANDLE_VALUE;
    }

    void Close()
    {
        m_File.Close();
    }

    FileRWT Read(void *Buffer, const FileRWT Size) const
    {
        FileRWT	BytesRead;
        if (ReadFile(m_File.Get(), Buffer, Size, &BytesRead, NULL) == 0)
            rad::ThrowWinError(TEXT("ReadFile : "));
            return BytesRead;
    }

    FileRWT Write(void const *Buffer, const FileRWT Size) const
    {
        FileRWT	BytesWritten;
        if (WriteFile(m_File.Get(), Buffer, Size, &BytesWritten, NULL) == 0)
            rad::ThrowWinError(TEXT("WriteFile : "));
            return BytesWritten;
    }

    FileSizeT GetSize() const
    {
        FileSizeT Size = {};

        Size.LowPart = GetFileSize(m_File.Get(), &Size.HighPart);

        if (Size.LowPart == 0xFFFFFFFF)
            rad::ThrowWinError(TEXT("GetFileSize : "));

        return Size;
    }

    DWORD GetType() const
    {
        return GetFileType(m_File.Get());
    }

    void SetTime(const FILETIME* lpCreationTime, const FILETIME* lpLastAccessTime, const FILETIME* lpLastWriteTime)
    {
        if (::SetFileTime(m_File.Get(), lpCreationTime, lpLastAccessTime, lpLastWriteTime) == 0)
            rad::ThrowWinError(TEXT("SetFileTime : "));
    }

    void GetTime(FILETIME* lpCreationTime, FILETIME* lpLastAccessTime, FILETIME* lpLastWriteTime) const
    {
        if (::GetFileTime(m_File.Get(), lpCreationTime, lpLastAccessTime, lpLastWriteTime) == 0)
            rad::ThrowWinError(TEXT("GetFileTime : "));
    }

private:
    rad::WinHandle	m_File;
};

#endif
