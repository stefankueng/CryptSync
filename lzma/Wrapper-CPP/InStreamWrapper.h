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
#include "../CPP/7zip/IStream.h"
#include "../CPP/Common/MyCom.h"

namespace SevenZip
{
class InStreamWrapper : public IInStream
    , public IStreamGetSize
{
private:
    long               m_refCount;
    CMyComPtr<IStream> m_baseStream;

public:
    InStreamWrapper(const CMyComPtr<IStream>& baseStream);
    virtual ~InStreamWrapper();

    STDMETHOD(QueryInterface)
    (REFIID iid, void** ppvObject);
    STDMETHOD_(ULONG, AddRef)
    ();
    STDMETHOD_(ULONG, Release)
    ();

    // ISequentialInStream
    STDMETHOD(Read)
    (void* data, UInt32 size, UInt32* processedSize);

    // IInStream
    STDMETHOD(Seek)
    (Int64 offset, UInt32 seekOrigin, UInt64* newPosition);

    // IStreamGetSize
    STDMETHOD(GetSize)
    (UInt64* size);
};
}
