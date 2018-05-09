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

#include "stdafx.h"
#include "InStreamWrapper.h"

namespace SevenZip
{
InStreamWrapper::InStreamWrapper(const CMyComPtr<IStream>& baseStream)
    : m_refCount(0)
    , m_baseStream(baseStream)
{
}

InStreamWrapper::~InStreamWrapper()
{
}

HRESULT STDMETHODCALLTYPE InStreamWrapper::QueryInterface(REFIID iid, void** ppvObject)
{
    if (iid == __uuidof(IUnknown))
    {
        *ppvObject = reinterpret_cast<IUnknown*>(this);
        AddRef();
        return S_OK;
    }

    if (iid == IID_ISequentialInStream)
    {
        *ppvObject = static_cast<ISequentialInStream*>(this);
        AddRef();
        return S_OK;
    }

    if (iid == IID_IInStream)
    {
        *ppvObject = static_cast<IInStream*>(this);
        AddRef();
        return S_OK;
    }

    if (iid == IID_IStreamGetSize)
    {
        *ppvObject = static_cast<IStreamGetSize*>(this);
        AddRef();
        return S_OK;
    }

    return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE InStreamWrapper::AddRef()
{
    return static_cast<ULONG>(InterlockedIncrement(&m_refCount));
}

ULONG STDMETHODCALLTYPE InStreamWrapper::Release()
{
    ULONG res = static_cast<ULONG>(InterlockedDecrement(&m_refCount));
    if (res == 0)
    {
        delete this;
    }
    return res;
}

STDMETHODIMP InStreamWrapper::Read(void* data, UInt32 size, UInt32* processedSize)
{
    ULONG   read = 0;
    HRESULT hr   = m_baseStream->Read(data, size, &read);
    if (processedSize != NULL)
    {
        *processedSize = read;
    }
    // Transform S_FALSE to S_OK
    return SUCCEEDED(hr) ? S_OK : hr;
}

STDMETHODIMP InStreamWrapper::Seek(Int64 offset, UInt32 seekOrigin, UInt64* newPosition)
{
    LARGE_INTEGER  move;
    ULARGE_INTEGER newPos;

    move.QuadPart = offset;
    HRESULT hr    = m_baseStream->Seek(move, seekOrigin, &newPos);
    if (newPosition != NULL)
    {
        *newPosition = newPos.QuadPart;
    }
    return hr;
}

STDMETHODIMP InStreamWrapper::GetSize(UInt64* size)
{
    STATSTG statInfo;
    HRESULT hr = m_baseStream->Stat(&statInfo, STATFLAG_NONAME);
    if (SUCCEEDED(hr))
    {
        *size = statInfo.cbSize.QuadPart;
    }
    return hr;
}
}
