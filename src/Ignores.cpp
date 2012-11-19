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

CIgnores::CIgnores(void)
{
    sIgnores = CRegStdString(L"Software\\CryptSync\\Ignores", L"*.tmp*|~*.*");
    stringtok(ignores, sIgnores, true);
    for (auto it = ignores.begin(); it != ignores.end(); ++it)
    {
        std::transform(it->begin(), it->end(), it->begin(), std::tolower);
    }
}


CIgnores::~CIgnores(void)
{
}

bool CIgnores::IsIgnored( const std::wstring& s )
{
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
