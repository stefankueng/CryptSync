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
#include <string>

namespace SevenZip
{
class ArchiveOpenCallback : public CallbackBase
    , public IArchiveOpenCallback
    , public ICryptoGetTextPassword
{
private:
    long m_refCount;

public:
    ArchiveOpenCallback();
    virtual ~ArchiveOpenCallback();

    STDMETHOD(QueryInterface)
    (REFIID iid, void** ppvObject);
    STDMETHOD_(ULONG, AddRef)
    ();
    STDMETHOD_(ULONG, Release)
    ();

    // IArchiveOpenCallback
    STDMETHOD(SetTotal)
    (const UInt64* files, const UInt64* bytes);
    STDMETHOD(SetCompleted)
    (const UInt64* files, const UInt64* bytes);

    // ICryptoGetTextPassword
    STDMETHOD(CryptoGetTextPassword)
    (BSTR* password);
};
}
