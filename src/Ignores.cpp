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
#include "StdAfx.h"
#include "Ignores.h"
#include "Registry.h"
#include "StringUtils.h"

#include <cctype>
#include <algorithm>

CIgnores* CIgnores::m_pInstance;

CIgnores::CIgnores(void)
{
    Reload();
}


CIgnores::~CIgnores(void)
{
    delete m_pInstance;
}

bool CIgnores::IsIgnored( const std::wstring& s )
{
    CAutoReadLock locker(m_guard);
    bool bIgnored = false;

    std::wstring scmp = s;
    std::transform(scmp.begin(), scmp.end(), scmp.begin(), std::tolower);
    size_t slashpos = s.find_last_of('\\');
    if (slashpos != std::string::npos)
        scmp = s.substr(slashpos+1);
    for (auto it = ignores.cbegin(); it != ignores.cend(); ++it)
    {
        if (wcswildcmp(it->c_str(), scmp.c_str()))
        {
            bIgnored = true;
            break;
        }
    }
    return bIgnored;
}

void CIgnores::Reload(const std::wstring& s /* = std::wstring() */)
{
    CAutoWriteLock locker(m_guard);
    ignores.clear();
    CRegStdString regIgnores(L"Software\\CryptSync\\Ignores", DEFAULT_IGNORES);
    if (s.empty())
        sIgnores = std::wstring(regIgnores);
    else
        sIgnores = s;
    stringtok(ignores, sIgnores, true);
    for (auto it = ignores.begin(); it != ignores.end(); ++it)
    {
        std::transform(it->begin(), it->end(), it->begin(), std::tolower);
    }
}

CIgnores& CIgnores::Instance()
{
    if (m_pInstance == NULL)
        m_pInstance = new CIgnores();
    return *m_pInstance;
}
