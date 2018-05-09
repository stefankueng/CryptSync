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
#include "ArchiveExtractCallback.h"
#include "OutStreamWrapper.h"
#include "Helper.h"
#include <comdef.h>
#include <Shlwapi.h>
#include "../CPP/Windows/PropVariant.h"

namespace SevenZip
{
const std::wstring EmptyFileAlias = L"[Content]";

ArchiveExtractCallback::ArchiveExtractCallback(const CMyComPtr<IInArchive>& archiveHandler, const std::wstring& directory, const std::wstring& password)
    : CallbackBase()
    , m_refCount(0)
    , m_archiveHandler(archiveHandler)
    , m_directory(directory)
{
    SetPassword(password);
}

ArchiveExtractCallback::~ArchiveExtractCallback()
{
}

STDMETHODIMP ArchiveExtractCallback::QueryInterface(REFIID iid, void** ppvObject)
{
    if (iid == __uuidof(IUnknown))
    {
        *ppvObject = reinterpret_cast<IUnknown*>(this);
        AddRef();
        return S_OK;
    }

    if (iid == IID_IArchiveExtractCallback)
    {
        *ppvObject = static_cast<IArchiveExtractCallback*>(this);
        AddRef();
        return S_OK;
    }

    if (iid == IID_ICryptoGetTextPassword)
    {
        *ppvObject = static_cast<ICryptoGetTextPassword*>(this);
        AddRef();
        return S_OK;
    }

    return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG)
ArchiveExtractCallback::AddRef()
{
    return static_cast<ULONG>(InterlockedIncrement(&m_refCount));
}

STDMETHODIMP_(ULONG)
ArchiveExtractCallback::Release()
{
    ULONG res = static_cast<ULONG>(InterlockedDecrement(&m_refCount));
    if (res == 0)
    {
        delete this;
    }
    return res;
}

STDMETHODIMP ArchiveExtractCallback::SetTotal(UInt64 size)
{
    //  - SetTotal is never called for ZIP and 7z formats
    m_total = size;
    if (m_callback)
        return m_callback(m_progress, m_total, m_progressPath);
    return S_OK;
}

STDMETHODIMP ArchiveExtractCallback::SetCompleted(const UInt64* completeValue)
{
    //Callback Event calls
    /*
    NB:
    - For ZIP format SetCompleted only called once per 1000 files in central directory and once per 100 in local ones.
    - For 7Z format SetCompleted is never called.
    */
    m_progress = *completeValue;
    if (m_callback)
        return m_callback(m_progress, m_total, m_progressPath);

    return S_OK;
}

STDMETHODIMP ArchiveExtractCallback::GetStream(UInt32 index, ISequentialOutStream** outStream, Int32 askExtractMode)
{
    try
    {
        // Retrieve all the various properties for the file at this index.
        GetPropertyFilePath(index);
        if (askExtractMode != NArchive::NExtract::NAskMode::kExtract)
        {
            return S_OK;
        }

        GetPropertyAttributes(index);
        GetPropertyIsDir(index);
        GetPropertyModifiedTime(index);
        GetPropertySize(index);
    }
    catch (_com_error& ex)
    {
        return ex.Error();
    }

    m_absPath = m_directory + L"\\" + m_relPath;

    if (m_isDir)
    {
        // Creating the directory here supports having empty directories.
        CreateRecursiveDirectory(m_absPath);
        *outStream = NULL;
        return S_OK;
    }

    std::wstring absDir = PathIsDirectory(m_absPath.c_str()) ? m_absPath : m_absPath.substr(0, m_absPath.find_last_of('\\'));
    CreateRecursiveDirectory(absDir);

    CMyComPtr<IStream> fileStream;
    if (FAILED(SHCreateStreamOnFileEx(m_absPath.c_str(), STGM_CREATE | STGM_WRITE, FILE_ATTRIBUTE_NORMAL, TRUE, NULL, &fileStream)))
    {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    CMyComPtr<OutStreamWrapper> wrapperStream = new OutStreamWrapper(fileStream);
    *outStream                                = wrapperStream.Detach();

    m_progressPath = m_absPath;
    if (m_callback)
        m_callback(m_progress, m_total, m_progressPath);

    return S_OK;
}

STDMETHODIMP ArchiveExtractCallback::PrepareOperation(Int32 /*askExtractMode*/)
{
    return S_OK;
}

STDMETHODIMP ArchiveExtractCallback::SetOperationResult(Int32 /*operationResult*/)
{
    if (m_absPath.empty())
    {
        if (m_callback)
            return m_callback(m_progress, m_total, m_progressPath);
        return S_OK;
    }

    if (m_hasModifiedTime)
    {
        HANDLE fileHandle = CreateFile(m_absPath.c_str(), GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (fileHandle != INVALID_HANDLE_VALUE)
        {
            SetFileTime(fileHandle, NULL, NULL, &m_modifiedTime);
            CloseHandle(fileHandle);
        }
    }

    if (m_hasAttrib)
    {
        SetFileAttributes(m_absPath.c_str(), m_attrib);
    }

    m_progressPath = m_absPath;
    if (m_callback)
        return m_callback(m_progress, m_total, m_progressPath);
    return S_OK;
}

STDMETHODIMP ArchiveExtractCallback::CryptoGetTextPassword(BSTR* password)
{
    *password = SysAllocString(m_password.c_str());
    return *password != 0 ? S_OK : E_OUTOFMEMORY;
}

void ArchiveExtractCallback::GetPropertyFilePath(UInt32 index)
{
    NWindows::NCOM::CPropVariant prop;
    HRESULT                      hr = m_archiveHandler->GetProperty(index, kpidPath, &prop);
    if (hr != S_OK)
    {
        _com_issue_error(hr);
    }

    if (prop.vt == VT_EMPTY)
    {
        m_relPath = EmptyFileAlias;
    }
    else if (prop.vt != VT_BSTR)
    {
        _com_issue_error(E_FAIL);
    }
    else
    {
        _bstr_t bstr = prop.bstrVal;
#ifdef _UNICODE
        m_relPath = bstr;
#else
        char relPath[MAX_PATH];
        int  size = WideCharToMultiByte(CP_UTF8, 0, bstr, bstr.length(), relPath, MAX_PATH, NULL, NULL);
        m_relPath.assign(relPath, size);
#endif
    }
}

void ArchiveExtractCallback::GetPropertyAttributes(UInt32 index)
{
    NWindows::NCOM::CPropVariant prop;
    HRESULT                      hr = m_archiveHandler->GetProperty(index, kpidAttrib, &prop);
    if (hr != S_OK)
    {
        _com_issue_error(hr);
    }

    if (prop.vt == VT_EMPTY)
    {
        m_attrib    = 0;
        m_hasAttrib = false;
    }
    else if (prop.vt != VT_UI4)
    {
        _com_issue_error(E_FAIL);
    }
    else
    {
        m_attrib    = prop.ulVal;
        m_hasAttrib = true;
    }
}

void ArchiveExtractCallback::GetPropertyIsDir(UInt32 index)
{
    NWindows::NCOM::CPropVariant prop;
    HRESULT                      hr = m_archiveHandler->GetProperty(index, kpidIsDir, &prop);
    if (hr != S_OK)
    {
        _com_issue_error(hr);
    }

    if (prop.vt == VT_EMPTY)
    {
        m_isDir = false;
    }
    else if (prop.vt != VT_BOOL)
    {
        _com_issue_error(E_FAIL);
    }
    else
    {
        m_isDir = prop.boolVal != VARIANT_FALSE;
    }
}

void ArchiveExtractCallback::GetPropertyModifiedTime(UInt32 index)
{
    NWindows::NCOM::CPropVariant prop;
    HRESULT                      hr = m_archiveHandler->GetProperty(index, kpidMTime, &prop);
    if (hr != S_OK)
    {
        _com_issue_error(hr);
    }

    if (prop.vt == VT_EMPTY)
    {
        m_hasModifiedTime = false;
    }
    else if (prop.vt != VT_FILETIME)
    {
        _com_issue_error(E_FAIL);
    }
    else
    {
        m_modifiedTime    = prop.filetime;
        m_hasModifiedTime = true;
    }
}

void ArchiveExtractCallback::GetPropertySize(UInt32 index)
{
    NWindows::NCOM::CPropVariant prop;
    HRESULT                      hr = m_archiveHandler->GetProperty(index, kpidSize, &prop);
    if (hr != S_OK)
    {
        _com_issue_error(hr);
    }

    switch (prop.vt)
    {
        case VT_EMPTY:
            m_hasNewFileSize = false;
            return;
        case VT_UI1:
            m_newFileSize = prop.bVal;
            break;
        case VT_UI2:
            m_newFileSize = prop.uiVal;
            break;
        case VT_UI4:
            m_newFileSize = prop.ulVal;
            break;
        case VT_UI8:
            m_newFileSize = (UInt64)prop.uhVal.QuadPart;
            break;
        default:
            _com_issue_error(E_FAIL);
    }

    m_hasNewFileSize = true;
}
}
