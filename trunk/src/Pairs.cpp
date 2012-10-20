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

#include "stdafx.h"
#include "resource.h"
#include "Pairs.h"
#include "Registry.h"
#include <algorithm>

CPairs::CPairs()
{
    InitPairList();
}

CPairs::~CPairs(void)
{
}


void CPairs::InitPairList()
{
    clear();
    int p = 0;
    for (;;)
    {
        WCHAR key[MAX_PATH];
        swprintf_s(key, L"Software\\CryptSync\\SyncPairOrig%d", p);
        CRegStdString origpathreg(key);
        std::wstring origpath = origpathreg;
        if (origpath.empty())
            break;

        swprintf_s(key, L"Software\\CryptSync\\SyncPairCrypt%d", p);
        CRegStdString cryptpathreg(key);
        std::wstring cryptpath = cryptpathreg;
        if (cryptpath.empty())
            break;

        auto t = std::make_tuple(origpath, cryptpath);
        if (std::find(cbegin(), cend(), t) == cend())
            push_back(t);
        ++p;
    }
}

void CPairs::SavePairs()
{
    int p = 0;
    for (auto it = cbegin(); (it != cend()) && (p < 200); ++it)
    {
        WCHAR key[MAX_PATH];
        swprintf_s(key, L"Software\\CryptSync\\SyncPairOrig%d", p);
        CRegStdString origpathreg(key);
        origpathreg = std::get<0>(*it);

        swprintf_s(key, L"Software\\CryptSync\\SyncPairCrypt%d", p);
        CRegStdString cryptpathreg(key);
        cryptpathreg = std::get<1>(*it);

        ++p;
    }
    // delete all possible remaining registry entries
    while (p < 200)
    {
        WCHAR key[MAX_PATH];
        swprintf_s(key, L"Software\\CryptSync\\SyncPairOrig%d", p);
        CRegStdString origpathreg(key);
        origpathreg.removeValue();

        swprintf_s(key, L"Software\\CryptSync\\SyncPairCrypt%d", p);
        CRegStdString cryptpathreg(key);
        cryptpathreg.removeValue();

        ++p;
    }
}

bool CPairs::AddPair( const std::wstring& orig, const std::wstring& crypt )
{
    auto t = std::make_tuple(orig, crypt);
    if (std::find(cbegin(), cend(), t) == cend())
    {
        push_back(t);
        return true;
    }

    return false;
}
