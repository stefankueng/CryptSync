// This file is based on the following file from the LZMA SDK (http://www.7-zip.org/sdk.html):
//   ./CPP/7zip/UI/Client7z/Client7z.cpp
#pragma once

#include "../CPP/7zip/Archive/IArchive.h"
#include "../CPP/7zip/IPassword.h"
#include <functional>
#include <string>

namespace SevenZip
{
class CallbackBase
{
protected:
    std::wstring                                                               m_password;
    std::function<HRESULT(UInt64 pos, UInt64 total, const std::wstring& path)> m_callback;
    UInt64                                                                     m_progress;
    UInt64                                                                     m_total;
    std::wstring                                                               m_progressPath;

public:
    void SetPassword(const std::wstring& pw) { m_password = pw; }
    void SetProgressCallback(const std::function<HRESULT(UInt64 pos, UInt64 total, const std::wstring& path)>& func) { m_callback = func; }

    CallbackBase();
    virtual ~CallbackBase();
};
}
