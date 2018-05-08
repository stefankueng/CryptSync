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
