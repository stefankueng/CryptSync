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

// This file is based on the following file from the LZMA SDK (http://www.7-zip.org/sdk.html):
//   ./CPP/7zip/UI/Client7z/Client7z.cpp
#pragma once
#include "CallbackBase.h"
#include "../CPP/7zip/Archive/IArchive.h"
#include "../CPP/7zip/IPassword.h"
#include "../CPP/Common/MyCom.h"

#include <string>

namespace SevenZip
{
class ArchiveExtractCallback : public CallbackBase
    , public IArchiveExtractCallback
    , public ICryptoGetTextPassword
{
private:
    long                  m_refCount;
    CMyComPtr<IInArchive> m_archiveHandler;
    std::wstring          m_directory;

    std::wstring m_relPath;
    std::wstring m_absPath;
    bool         m_isDir;

    bool   m_hasAttrib;
    UInt32 m_attrib;

    bool     m_hasModifiedTime;
    FILETIME m_modifiedTime;

    bool   m_hasNewFileSize;
    UInt64 m_newFileSize;

public:
    ArchiveExtractCallback(const CMyComPtr<IInArchive>& archiveHandler, const std::wstring& directory, const std::wstring& password);
    virtual ~ArchiveExtractCallback();

    STDMETHOD(QueryInterface)
    (REFIID iid, void** ppvObject);
    STDMETHOD_(ULONG, AddRef)
    ();
    STDMETHOD_(ULONG, Release)
    ();

    // IProgress
    STDMETHOD(SetTotal)
    (UInt64 size);
    STDMETHOD(SetCompleted)
    (const UInt64* completeValue);

    // IArchiveExtractCallback
    STDMETHOD(GetStream)
    (UInt32 index, ISequentialOutStream** outStream, Int32 askExtractMode);
    STDMETHOD(PrepareOperation)
    (Int32 askExtractMode);
    STDMETHOD(SetOperationResult)
    (Int32 resultEOperationResult);

    // ICryptoGetTextPassword
    STDMETHOD(CryptoGetTextPassword)
    (BSTR* password);

private:
    void GetPropertyFilePath(UInt32 index);
    void GetPropertyAttributes(UInt32 index);
    void GetPropertyIsDir(UInt32 index);
    void GetPropertyModifiedTime(UInt32 index);
    void GetPropertySize(UInt32 index);
};
}
