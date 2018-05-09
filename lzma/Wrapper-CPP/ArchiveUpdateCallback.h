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

#include "C7Zip.h"
#include "CallbackBase.h"
#include "../CPP/7zip/Archive/IArchive.h"
#include "../CPP/7zip/ICoder.h"
#include "../CPP/7zip/IPassword.h"
#include <vector>

namespace SevenZip
{
class ArchiveUpdateCallback : public CallbackBase
    , public IArchiveUpdateCallback
    , public ICryptoGetTextPassword2
    , public ICompressProgressInfo
{
private:
    long                             m_refCount;
    std::wstring                     m_dirPrefix;
    std::wstring                     m_outputPath;
    const std::vector<FilePathInfo>& m_filePaths;

public:
    ArchiveUpdateCallback(const std::wstring& dirPrefix, const std::vector<FilePathInfo>& filePaths, const std::wstring& outputFilePath, const std::wstring& password);
    virtual ~ArchiveUpdateCallback();

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

    // IArchiveUpdateCallback
    STDMETHOD(GetUpdateItemInfo)
    (UInt32 index, Int32* newData, Int32* newProperties, UInt32* indexInArchive);
    STDMETHOD(GetProperty)
    (UInt32 index, PROPID propID, PROPVARIANT* value);
    STDMETHOD(GetStream)
    (UInt32 index, ISequentialInStream** inStream);
    STDMETHOD(SetOperationResult)
    (Int32 operationResult);

    // ICryptoGetTextPassword2
    STDMETHOD(CryptoGetTextPassword2)
    (Int32* passwordIsDefined, BSTR* password);

    // ICompressProgressInfo
    STDMETHOD(SetRatioInfo)
    (const UInt64* inSize, const UInt64* outSize);
};
}
