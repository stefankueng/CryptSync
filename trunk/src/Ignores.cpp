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

template <typename Container>
void stringtok(Container &container, const std::wstring  &in, bool trim,
    const wchar_t * const delimiters = L"|")
{
    const std::string::size_type len = in.length();
    std::string::size_type i = 0;

    while ( i < len )
    {
        if (trim)
        {
            // eat leading whitespace
            i = in.find_first_not_of (delimiters, i);
            if (i == std::string::npos)
                return;   // nothing left but white space
        }

        // find the end of the token
        std::string::size_type j = in.find_first_of (delimiters, i);

        // push token
        if (j == std::string::npos) {
            container.push_back (in.substr(i));
            return;
        } else
            container.push_back (in.substr(i, j-i));

        // set up for next loop
        i = j + 1;
    }
}


CIgnores::CIgnores(void)
{
    sIgnores = CRegStdString(L"Software\\CryptSync\\Ignores", L"*.tmp|~*.*");
    stringtok(ignores, sIgnores, true);
}


CIgnores::~CIgnores(void)
{
}

bool CIgnores::IsIgnored( const std::wstring& s )
{
    bool bIgnored = false;

    std::wstring scmp = s;
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
