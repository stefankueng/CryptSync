// CryptSync - A folder sync tool with encryption

// Copyright (C) 2018 - Stefan Kueng

// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
//

#pragma once
#include "InStreamWrapper.h"
#include "ArchiveOpenCallback.h"
#include "../CPP/Common/MyCom.h"
#include "../CPP/7zip/Archive/IArchive.h"
#include "../CPP/Windows/PropVariant.h"

#include <string>
#include <functional>
#include <Shlwapi.h>

using namespace SevenZip;

STDAPI CreateObject(const GUID* clsid, const GUID* iid, void** outObject);

enum class CompressionFormat : int
{
    Unknown,
    SevenZip,
    Zip,
    GZip,
    BZip2,
    Rar,
    Tar,
    Iso,
    Cab,
    Lzma,
    Lzma86,
    Arj,
    Z,
    Lzh,
    Nsis,
    Xz,
    Ppmd,
    Rar5,
    Chm,
    Last
};

class ArchiveFile
{
public:
    std::wstring name;
    size_t       uncompressedSize;
};

struct FileInfo
{
    std::wstring FileName;
    FILETIME     LastWriteTime;
    FILETIME     CreationTime;
    FILETIME     LastAccessTime;
    ULONGLONG    Size;
    UINT         Attributes;
    bool         IsDirectory;
};

struct FilePathInfo : public FileInfo
{
    std::wstring FilePath;
};

class C7Zip
{
public:
    C7Zip();
    ~C7Zip();

    /// Set the path of the compressed file.
    void SetArchivePath(const std::wstring& path)
    {
        m_archivePath       = path;
        m_compressionFormat = GetCompressionFormatFromPath();
    }

    /// Sets a password for encryption or decryption. Only used for
    /// archive formats that support it.
    void SetPassword(const std::wstring& pw) { m_password = pw; }

    /// Sets the compression format to use, and the compression level between 0-9.
    /// for unpacking, if the format is not set (CompressionFormat::Unknown) or
    /// opening the archive fails with the compression format set,
    /// all available compression formats are tried in sequence until opening
    /// the archive succeeds.
    void SetCompressionFormat(CompressionFormat f, int compressionlevel)
    {
        m_compressionFormat = f;
        m_compressionLevel  = compressionlevel;
    }

    /// sets a callback function that can be used to show progress info
    /// and/or to cancel the operation. Return S_OK from the callback
    /// to continue, or E_ABORT to cancel.
    void SetCallback(const std::function<HRESULT(UInt64 pos, UInt64 total, const std::wstring& path)>& callback) { m_callback = callback; }

    /// Add paths to compress into the archive file.
    /// if the path ends with a backslash, the directory is not added itselb but only
    /// the contents.
    bool AddPath(const std::wstring& path);

    /// Extracts the contents of the archive to the destPath.
    bool Extract(const std::wstring& destPath);

    /// Lists all files inside an archive to the container. container can be std:vector, std::list, ...
    template <class Container>
    bool ListFiles(Container& container)
    {
        CMyComPtr<IStream> fileStream;
        const WCHAR*       filePathStr = m_archivePath.c_str();
        if (FAILED(SHCreateStreamOnFileEx(filePathStr, STGM_READ | STGM_SHARE_DENY_NONE, FILE_ATTRIBUTE_NORMAL, FALSE, NULL, &fileStream)))
        {
            return false;
        }

        CMyComPtr<IInArchive> archive;
        HRESULT               hr     = S_FALSE;
        auto                  guid   = GetGUIDFromFormat(m_compressionFormat);
        bool                  bTried = false;
        if (guid == nullptr)
        {
            guid   = GetGUIDByTrying(m_compressionFormat, fileStream);
            bTried = true;
        }

        hr = CreateObject(guid, &IID_IInArchive, reinterpret_cast<void**>(&archive));

        CMyComPtr<InStreamWrapper>     inFile       = new InStreamWrapper(fileStream);
        CMyComPtr<ArchiveOpenCallback> openCallback = new ArchiveOpenCallback();
        openCallback->SetPassword(m_password);
        openCallback->SetProgressCallback(m_callback);

        hr = archive->Open(inFile, 0, openCallback);
        if (hr != S_OK)
        {
            if (bTried)
                return false;
            guid = GetGUIDByTrying(m_compressionFormat, fileStream);
            if (guid == nullptr)
                return false;
            hr = CreateObject(guid, &IID_IInArchive, reinterpret_cast<void**>(&archive));

            hr = archive->Open(inFile, 0, openCallback);
            if (hr != S_OK)
                return false;
        }

        // List command
        UInt32 numItems = 0;
        archive->GetNumberOfItems(&numItems);
        for (UInt32 i = 0; i < numItems; i++)
        {
            {
                ArchiveFile fileInfo;
                // Get uncompressed size of file
                NWindows::NCOM::CPropVariant prop;
                archive->GetProperty(i, kpidSize, &prop);

                fileInfo.uncompressedSize = prop.intVal;

                // Get name of file
                archive->GetProperty(i, kpidPath, &prop);

                //valid string? pass back the found value and call the callback function if set
                if (prop.vt == VT_BSTR)
                {
                    WCHAR* path   = prop.bstrVal;
                    fileInfo.name = path;
                    container.push_back(fileInfo);
                }
            }
        }
        return !container.empty();
    }

private:
    CompressionFormat GetCompressionFormatFromPath();
    const GUID*       GetGUIDFromFormat(CompressionFormat format);
    const GUID* GetGUIDByTrying(CompressionFormat& format, CMyComPtr<IStream>& fileStream);

private:
    std::wstring                                                               m_archivePath;
    std::wstring                                                               m_password;
    CompressionFormat                                                          m_compressionFormat;
    int                                                                        m_compressionLevel;
    std::function<HRESULT(UInt64 pos, UInt64 total, const std::wstring& path)> m_callback;
};
