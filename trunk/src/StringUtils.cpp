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
#include "StringUtils.h"

int strwildcmp(const char *wild, const char *string)
{
    const char *cp = NULL;
    const char *mp = NULL;
    while ((*string) && (*wild != '*'))
    {
        if ((*wild != *string) && (*wild != '?'))
        {
            return 0;
        }
        wild++;
        string++;
    }
    while (*string)
    {
        if (*wild == '*')
        {
            if (!*++wild)
            {
                return 1;
            }
            mp = wild;
            cp = string+1;
        }
        else if ((*wild == *string) || (*wild == '?'))
        {
            wild++;
            string++;
        }
        else
        {
            wild = mp;
            string = cp++;
        }
    }

    while (*wild == '*')
    {
        wild++;
    }
    return !*wild;
}

int wcswildcmp(const wchar_t *wild, const wchar_t *string)
{
    const wchar_t *cp = NULL;
    const wchar_t *mp = NULL;
    while ((*string) && (*wild != '*'))
    {
        if ((*wild != *string) && (*wild != '?'))
        {
            return 0;
        }
        wild++;
        string++;
    }
    while (*string)
    {
        if (*wild == '*')
        {
            if (!*++wild)
            {
                return 1;
            }
            mp = wild;
            cp = string+1;
        }
        else if ((*wild == *string) || (*wild == '?'))
        {
            wild++;
            string++;
        }
        else
        {
            wild = mp;
            string = cp++;
        }
    }

    while (*wild == '*')
    {
        wild++;
    }
    return !*wild;
}

BYTE HexLookup[513] = {
    "000102030405060708090a0b0c0d0e0f"
    "101112131415161718191a1b1c1d1e1f"
    "202122232425262728292a2b2c2d2e2f"
    "303132333435363738393a3b3c3d3e3f"
    "404142434445464748494a4b4c4d4e4f"
    "505152535455565758595a5b5c5d5e5f"
    "606162636465666768696a6b6c6d6e6f"
    "707172737475767778797a7b7c7d7e7f"
    "808182838485868788898a8b8c8d8e8f"
    "909192939495969798999a9b9c9d9e9f"
    "a0a1a2a3a4a5a6a7a8a9aaabacadaeaf"
    "b0b1b2b3b4b5b6b7b8b9babbbcbdbebf"
    "c0c1c2c3c4c5c6c7c8c9cacbcccdcecf"
    "d0d1d2d3d4d5d6d7d8d9dadbdcdddedf"
    "e0e1e2e3e4e5e6e7e8e9eaebecedeeef"
    "f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff"
};
BYTE DecLookup[] = {
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // gap before first hex digit
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 
    0,1,2,3,4,5,6,7,8,9,       // 0123456789
    0,0,0,0,0,0,0,             // :;<=>?@ (gap)
    10,11,12,13,14,15,         // ABCDEF 
    0,0,0,0,0,0,0,0,0,0,0,0,0, // GHIJKLMNOPQRS (gap)
    0,0,0,0,0,0,0,0,0,0,0,0,0, // TUVWXYZ[/]^_` (gap)
    10,11,12,13,14,15          // abcdef 
};

std::string CStringUtils::ToHexString( BYTE* pSrc, int nSrcLen )
{
    WORD * pwHex=  (WORD*)HexLookup;
    std::unique_ptr<char[]> dest(new char[(nSrcLen*2)+1]);
    WORD * pwDest = (WORD*)dest.get();
    for (int j=0; j<nSrcLen; j++ )
    {
        *pwDest= pwHex[*pSrc];
        pwDest++; pSrc++;
    }
    *((BYTE*)pwDest)= 0;  // terminate the string
    return std::string(dest.get());
}

bool CStringUtils::FromHexString( const std::string& src, BYTE* pDest )
{
    if (src.size() % 2)
        return false;
    for (auto it = src.cbegin(); it != src.cend(); ++it)
    {
        if ((*it < '0') || (*it > 'f'))
            return false;
        int d =  DecLookup[*it] << 4;
        ++it;
        d |= DecLookup[*it];
        *pDest++ = (BYTE)d;
    }
    return true;
}

std::wstring CStringUtils::ToHexWString( BYTE* pSrc, int nSrcLen )
{
    std::string s = ToHexString(pSrc, nSrcLen);
    return std::wstring(s.begin(), s.end());
}

bool CStringUtils::FromHexString( const std::wstring& src, BYTE* pDest )
{
    std::string s = std::string(src.begin(), src.end());
    return FromHexString(s, pDest);
}

