// CryptSync - A folder sync tool with encryption

// Copyright (C) 2012 - Stefan Kueng

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

#include "ReaderWriterLock.h"

#define DEFAULT_IGNORES L"*.tmp*|~*.*|thumbs.db|desktop.ini"


class CIgnores
{
public:
    static CIgnores& Instance();
    bool IsIgnored(const std::wstring& s);
    void Reload(const std::wstring& s = std::wstring());
private:
    CIgnores(void);
    ~CIgnores(void);


private:
    static CIgnores *               m_pInstance;
    CReaderWriterLock               m_guard;
    std::wstring                    sIgnores;
    std::vector<std::wstring>       ignores;
};
