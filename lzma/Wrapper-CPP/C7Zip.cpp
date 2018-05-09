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

#include "StdAfx.h"
#include "C7Zip.h"
#include "GUIDs.h"
#include "OutStreamWrapper.h"
#include "ArchiveUpdateCallback.h"
#include "DirFileEnum.h"
#include "ArchiveExtractCallback.h"
#include "Helper.h"
#include "../CPP/7zip/IDecl.h"
#include "../CPP/Windows/PropVariant.h"


C7Zip::C7Zip()
    : m_compressionFormat(CompressionFormat::Unknown)
    , m_compressionLevel(5)
    , m_callback(nullptr)
{
}

C7Zip::~C7Zip()
{
}

CompressionFormat C7Zip::GetCompressionFormatFromPath()
{
    auto dotPos = m_archivePath.find_last_of('.');
    if (dotPos == std::wstring::npos)
        return CompressionFormat::Unknown;
    if (dotPos >= m_archivePath.size())
        return CompressionFormat::Unknown;
    auto ext = m_archivePath.substr(dotPos + 1);

    if (wcsicmp(ext.c_str(), L"zip") == 0)
        return CompressionFormat::Zip;

    if (wcsicmp(ext.c_str(), L"7z") == 0)
        return CompressionFormat::SevenZip;
    if (wcsicmp(ext.c_str(), L"7zip") == 0)
        return CompressionFormat::SevenZip;

    if (wcsicmp(ext.c_str(), L"tar") == 0)
        return CompressionFormat::Tar;

    if (wcsicmp(ext.c_str(), L"bz2") == 0)
        return CompressionFormat::BZip2;

    if (wcsicmp(ext.c_str(), L"rar") == 0)
        return CompressionFormat::Rar;

    if (wcsicmp(ext.c_str(), L"iso") == 0)
        return CompressionFormat::Iso;

    if (wcsicmp(ext.c_str(), L"cab") == 0)
        return CompressionFormat::Cab;

    if (wcsicmp(ext.c_str(), L"lzma") == 0)
        return CompressionFormat::Lzma;

    if (wcsicmp(ext.c_str(), L"lzma86") == 0)
        return CompressionFormat::Lzma86;

    if (wcsicmp(ext.c_str(), L"chm") == 0)
        return CompressionFormat::Chm;

    return CompressionFormat::Unknown;
}

const GUID* C7Zip::GetGUIDFromFormat(CompressionFormat format)
{
    switch (format)
    {
        case CompressionFormat::Unknown:
        default:
            return nullptr;
        case CompressionFormat::SevenZip:
            return &CLSID_CFormat7z;
        case CompressionFormat::Zip:
            return &CLSID_CFormatZip;
        case CompressionFormat::GZip:
            return &CLSID_CFormatGZip;
        case CompressionFormat::BZip2:
            return &CLSID_CFormatBZip2;
        case CompressionFormat::Rar:
            return &CLSID_CFormatRar;
        case CompressionFormat::Tar:
            return &CLSID_CFormatTar;
        case CompressionFormat::Iso:
            return &CLSID_CFormatIso;
        case CompressionFormat::Cab:
            return &CLSID_CFormatCab;
        case CompressionFormat::Lzma:
            return &CLSID_CFormatLzma;
        case CompressionFormat::Lzma86:
            return &CLSID_CFormatLzma86;
        case CompressionFormat::Arj:
            return &CLSID_CFormatArj;
        case CompressionFormat::Z:
            return &CLSID_CFormatZ;
        case CompressionFormat::Lzh:
            return &CLSID_CFormatLzh;
        case CompressionFormat::Nsis:
            return &CLSID_CFormatNsis;
        case CompressionFormat::Xz:
            return &CLSID_CFormatXz;
        case CompressionFormat::Ppmd:
            return &CLSID_CFormatPpmd;
        case CompressionFormat::Rar5:
            return &CLSID_CFormatRar5;
        case CompressionFormat::Chm:
            return &CLSID_CFormatChm;
    }
}

const GUID* C7Zip::GetGUIDByTrying(CompressionFormat& format, CMyComPtr<IStream>& fileStream)
{
    CMyComPtr<ArchiveOpenCallback> openCallback = new ArchiveOpenCallback();
    openCallback->SetPassword(m_password);
    for (auto i = (int)CompressionFormat::SevenZip; i < (int)CompressionFormat::Last; ++i)
    {
        auto guid = GetGUIDFromFormat((CompressionFormat)i);
        if (guid)
        {
            CMyComPtr<IInArchive> archive;

            auto hr = CreateObject(guid, &IID_IInArchive, reinterpret_cast<void**>(&archive));

            CMyComPtr<InStreamWrapper> inFile = new InStreamWrapper(fileStream);

            hr = archive->Open(inFile, 0, openCallback);
            if (hr == S_OK)
            {
                format = (CompressionFormat)i;
                return guid;
            }
        }
    }
    format = CompressionFormat::Unknown;
    return nullptr;
}

bool C7Zip::AddPath(const std::wstring& path)
{
    std::vector<FilePathInfo> filePaths;

    if (!PathIsDirectory(path.c_str()))
    {
        WIN32_FILE_ATTRIBUTE_DATA fi = {0};
        GetFileAttributesEx(path.c_str(), GetFileExInfoStandard, &fi);
        FilePathInfo fpi;
        fpi.FilePath       = path;
        fpi.FileName       = path.substr(path.find_last_of('\\') + 1);
        fpi.Attributes     = fi.dwFileAttributes;
        fpi.CreationTime   = fi.ftCreationTime;
        fpi.IsDirectory    = false;
        fpi.LastAccessTime = fi.ftLastAccessTime;
        fpi.LastWriteTime  = fi.ftLastWriteTime;
        fpi.Size           = (static_cast<ULONGLONG>(fi.nFileSizeHigh) << sizeof(fi.nFileSizeLow) * 8) | fi.nFileSizeLow;
        filePaths.push_back(fpi);
    }
    else
    {
        CDirFileEnum enumerator(path);
        bool         isDir = false;
        std::wstring enumPath;
        while (enumerator.NextFile(enumPath, &isDir, true))
        {
            auto         fi = enumerator.GetFileInfo();
            FilePathInfo fpi;
            fpi.FilePath       = enumPath;
            fpi.FileName       = enumPath.substr(enumPath.find_last_of('\\') + 1);
            fpi.Attributes     = fi->dwFileAttributes;
            fpi.CreationTime   = fi->ftCreationTime;
            fpi.IsDirectory    = isDir;
            fpi.LastAccessTime = fi->ftLastAccessTime;
            fpi.LastWriteTime  = fi->ftLastWriteTime;
            fpi.Size           = (static_cast<ULONGLONG>(fi->nFileSizeHigh) << sizeof(fi->nFileSizeLow) * 8) | fi->nFileSizeLow;
            filePaths.push_back(fpi);
        }
    }

    CMyComPtr<IOutArchive> archive;
    HRESULT                hr   = S_FALSE;
    auto                   guid = GetGUIDFromFormat(m_compressionFormat);
    if (guid == nullptr)
        return false;

    hr = CreateObject(guid, &IID_IOutArchive, reinterpret_cast<void**>(&archive));
    if (FAILED(hr))
        return false;

    // set the compression properties
    bool                         encryptHeaders = (m_compressionFormat == CompressionFormat::SevenZip && !m_password.empty());
    const size_t                 numProps       = 2;
    const wchar_t*               names[numProps];  // = { L"x" };
    NWindows::NCOM::CPropVariant values[numProps]; // = { static_cast<UInt32>(m_compressionLevel) };
    names[0]  = L"x";
    values[0] = static_cast<UInt32>(m_compressionLevel);
    if (encryptHeaders)
    {
        names[1]  = L"he";
        values[1] = true;
    }

    CMyComPtr<ISetProperties> setter;
    archive->QueryInterface(IID_ISetProperties, reinterpret_cast<void**>(&setter));
    if (setter != nullptr)
    {
        hr = setter->SetProperties(names, values, encryptHeaders ? 2 : 1);
        if (hr != S_OK)
        {
            return false;
        }
    }

    CMyComPtr<IStream> fileStream;
    const WCHAR*       filePathStr = m_archivePath.c_str();
    if (FAILED(SHCreateStreamOnFileEx(filePathStr, STGM_CREATE | STGM_WRITE, FILE_ATTRIBUTE_NORMAL, TRUE, NULL, &fileStream)))
    {
        CreateRecursiveDirectory(m_archivePath.substr(0, m_archivePath.find_last_of('\\')));
        if (FAILED(SHCreateStreamOnFileEx(filePathStr, STGM_CREATE | STGM_WRITE, FILE_ATTRIBUTE_NORMAL, TRUE, NULL, &fileStream)))
        {
            return false;
        }
    }

    CMyComPtr<OutStreamWrapper> outFile   = new OutStreamWrapper(fileStream);
    std::wstring                dirPrefix = path;
    if (*path.rbegin() != '\\')
    {
        dirPrefix = dirPrefix.substr(0, dirPrefix.find_last_of('\\') + 1);
    }
    CMyComPtr<ArchiveUpdateCallback> updateCallback = new ArchiveUpdateCallback(dirPrefix, filePaths, m_archivePath, m_password);
    updateCallback->SetProgressCallback(m_callback);

    return SUCCEEDED(archive->UpdateItems(outFile, (UInt32)filePaths.size(), updateCallback));
}

bool C7Zip::Extract(const std::wstring& destPath)
{
    CMyComPtr<IStream> fileStream;
    const WCHAR*       filePathStr = m_archivePath.c_str();
    if (FAILED(SHCreateStreamOnFileEx(filePathStr, STGM_READ, FILE_ATTRIBUTE_NORMAL, FALSE, NULL, &fileStream)))
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

    CMyComPtr<ArchiveExtractCallback> extractCallback = new ArchiveExtractCallback(archive, destPath, m_password);
    extractCallback->SetProgressCallback(m_callback);

    hr = archive->Extract(NULL, (UInt32)-1, false, extractCallback);
    if (hr != S_OK) // returning S_FALSE also indicates error
    {
        return false; //Extract archive error
    }
    archive->Close();
    return true;
}
