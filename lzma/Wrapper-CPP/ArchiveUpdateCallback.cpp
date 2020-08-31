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
#include "StdAfx.h"
#include "ArchiveUpdateCallback.h"
#include "InStreamWrapper.h"
#include "../CPP/Common/MyCom.h"

#include <Shlwapi.h>
#include "../CPP/Windows/PropVariant.h"

namespace SevenZip
{
ArchiveUpdateCallback::ArchiveUpdateCallback(const std::wstring& dirPrefix, const std::vector<FilePathInfo>& filePaths, const std::wstring& outputFilePath, const std::wstring& password)
    : CallbackBase()
    , m_refCount(0)
    , m_dirPrefix(dirPrefix)
    , m_filePaths(filePaths)
    , m_outputPath(outputFilePath)
{
    SetPassword(password);
}

ArchiveUpdateCallback::~ArchiveUpdateCallback()
{
}

STDMETHODIMP ArchiveUpdateCallback::QueryInterface(REFIID iid, void** ppvObject)
{
    if (iid == __uuidof(IUnknown))
    {
        *ppvObject = reinterpret_cast<IUnknown*>(this);
        AddRef();
        return S_OK;
    }

    if (iid == IID_IArchiveUpdateCallback)
    {
        *ppvObject = static_cast<IArchiveUpdateCallback*>(this);
        AddRef();
        return S_OK;
    }

    if (iid == IID_ICryptoGetTextPassword2)
    {
        *ppvObject = static_cast<ICryptoGetTextPassword2*>(this);
        AddRef();
        return S_OK;
    }

    if (iid == IID_ICompressProgressInfo)
    {
        *ppvObject = static_cast<ICompressProgressInfo*>(this);
        AddRef();
        return S_OK;
    }

    return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG)
ArchiveUpdateCallback::AddRef()
{
    return static_cast<ULONG>(InterlockedIncrement(&m_refCount));
}

STDMETHODIMP_(ULONG)
ArchiveUpdateCallback::Release()
{
    ULONG res = static_cast<ULONG>(InterlockedDecrement(&m_refCount));
    if (res == 0)
    {
        delete this;
    }
    return res;
}

STDMETHODIMP ArchiveUpdateCallback::SetTotal(UInt64 size)
{
    m_total = size;
    if (m_callback)
        return m_callback(m_progress, m_total, m_progressPath);

    return S_OK;
}

STDMETHODIMP ArchiveUpdateCallback::SetCompleted(const UInt64* completeValue)
{
    if (completeValue == nullptr)
        return E_FAIL;
    m_progress = *completeValue;
    if (m_callback)
        return m_callback(m_progress, m_total, m_progressPath);
    return S_OK;
}

STDMETHODIMP ArchiveUpdateCallback::GetUpdateItemInfo(UInt32 /*index*/, Int32* newData, Int32* newProperties, UInt32* indexInArchive)
{
    // Setting info for Create mode (vs. Append mode).
    // TODO: support append mode
    if (newData != NULL)
    {
        *newData = 1;
    }

    if (newProperties != NULL)
    {
        *newProperties = 1;
    }

    if (indexInArchive != NULL)
    {
        *indexInArchive = static_cast<UInt32>(-1); // TODO: UInt32.Max
    }

    return S_OK;
}

STDMETHODIMP ArchiveUpdateCallback::GetProperty(UInt32 index, PROPID propID, PROPVARIANT* value)
{
    NWindows::NCOM::CPropVariant prop;

    if (propID == kpidIsAnti)
    {
        prop = false;
        prop.Detach(value);
        return S_OK;
    }

    if (index >= m_filePaths.size())
    {
        return E_INVALIDARG;
    }

    const FilePathInfo& fileInfo = m_filePaths.at(index);
    switch (propID)
    {
        case kpidPath:
            prop = fileInfo.FilePath.substr(m_dirPrefix.size(), fileInfo.FilePath.size() - m_dirPrefix.size()).c_str();
            break;
        case kpidIsDir:
            prop = fileInfo.IsDirectory;
            break;
        case kpidSize:
            prop = fileInfo.Size;
            break;
        case kpidAttrib:
            prop = fileInfo.Attributes;
            break;
        case kpidCTime:
            prop = fileInfo.CreationTime;
            break;
        case kpidATime:
            prop = fileInfo.LastAccessTime;
            break;
        case kpidMTime:
            prop = fileInfo.LastWriteTime;
            break;
    }

    prop.Detach(value);
    return S_OK;
}

STDMETHODIMP ArchiveUpdateCallback::GetStream(UInt32 index, ISequentialInStream** inStream)
{
    if (index >= m_filePaths.size())
    {
        return E_INVALIDARG;
    }

    const FilePathInfo& fileInfo = m_filePaths.at(index);
    m_progressPath               = fileInfo.FilePath;
    if (m_callback)
        m_callback(m_progress, m_total, m_progressPath);
    if (fileInfo.IsDirectory)
    {
        return S_OK;
    }

    CMyComPtr<IStream> fileStream;
    if (FAILED(SHCreateStreamOnFileEx(fileInfo.FilePath.c_str(), STGM_READ | STGM_SHARE_DENY_NONE, FILE_ATTRIBUTE_NORMAL, FALSE, NULL, &fileStream)))
    {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    CMyComPtr<InStreamWrapper> wrapperStream = new InStreamWrapper(fileStream);
    *inStream                                = wrapperStream.Detach();

    return S_OK;
}

STDMETHODIMP ArchiveUpdateCallback::SetOperationResult(Int32 /*operationResult*/)
{
    return S_OK;
}

STDMETHODIMP ArchiveUpdateCallback::CryptoGetTextPassword2(Int32* passwordIsDefined, BSTR* password)
{
    if (m_password.empty())
    {
        *passwordIsDefined = 0;
        *password          = SysAllocString(L"");
        return *password != 0 ? S_OK : E_OUTOFMEMORY;
    }
    else
    {
        *passwordIsDefined = 1;
        *password          = SysAllocString(m_password.c_str());
        return *password != 0 ? S_OK : E_OUTOFMEMORY;
    }
}

STDMETHODIMP ArchiveUpdateCallback::SetRatioInfo(const UInt64* /*inSize*/, const UInt64* /*outSize*/)
{
    return S_OK;
}
}
