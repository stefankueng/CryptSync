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
#include "ArchiveOpenCallback.h"

namespace SevenZip
{
ArchiveOpenCallback::ArchiveOpenCallback()
    : CallbackBase()
    , m_refCount(0)
{
}

ArchiveOpenCallback::~ArchiveOpenCallback()
{
}

STDMETHODIMP ArchiveOpenCallback::QueryInterface(REFIID iid, void** ppvObject)
{
    if (iid == __uuidof(IUnknown))
    {
        *ppvObject = reinterpret_cast<IUnknown*>(this);
        AddRef();
        return S_OK;
    }

    if (iid == IID_IArchiveOpenCallback)
    {
        *ppvObject = static_cast<IArchiveOpenCallback*>(this);
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
ArchiveOpenCallback::AddRef()
{
    return static_cast<ULONG>(InterlockedIncrement(&m_refCount));
}

STDMETHODIMP_(ULONG)
ArchiveOpenCallback::Release()
{
    ULONG res = static_cast<ULONG>(InterlockedDecrement(&m_refCount));
    if (res == 0)
    {
        delete this;
    }
    return res;
}

STDMETHODIMP ArchiveOpenCallback::SetTotal(const UInt64* files, const UInt64* /*bytes*/)
{
    if (files == nullptr)
        return E_FAIL;
    m_progress = *files;
    if (m_callback)
        return m_callback(m_progress, m_total, m_progressPath);
    return S_OK;
}

STDMETHODIMP ArchiveOpenCallback::SetCompleted(const UInt64* files, const UInt64* /*bytes*/)
{
    if (files == nullptr)
        return E_FAIL;
    m_progress = *files;
    if (m_callback)
        return m_callback(m_progress, m_total, m_progressPath);
    return S_OK;
}

STDMETHODIMP ArchiveOpenCallback::CryptoGetTextPassword(BSTR* password)
{
    *password = SysAllocString(m_password.c_str());
    return *password != 0 ? S_OK : E_OUTOFMEMORY;
}
}
