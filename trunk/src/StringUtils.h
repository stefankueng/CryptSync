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
#include <string>
#include <algorithm>

class CStringUtils
{
public:
    // trim from both ends
    static inline std::string &trim(std::string &s) {
        return ltrim(rtrim(s));
    }

    // trim from start
    static inline std::string &ltrim(std::string &s) {
        s.erase(s.begin(), std::find_if(s.begin(), s.end(), std::not1(std::ptr_fun<int, int>(isspace))));
        return s;
    }

    // trim from end
    static inline std::string &rtrim(std::string &s) {
        s.erase(std::find_if(s.rbegin(), s.rend(), std::not1(std::ptr_fun<int, int>(isspace))).base(), s.end());
        return s;
    }

    // trim from both ends
    static inline std::wstring &trim(std::wstring &s) {
        return ltrim(rtrim(s));
    }

    // trim from start
    static inline std::wstring &ltrim(std::wstring &s) {
        s.erase(s.begin(), std::find_if(s.begin(), s.end(), std::not1(std::ptr_fun<wint_t, int>(iswspace))));
        return s;
    }

    // trim from end
    static inline std::wstring &rtrim(std::wstring &s) {
        s.erase(std::find_if(s.rbegin(), s.rend(), std::not1(std::ptr_fun<wint_t, int>(iswspace))).base(), s.end());
        return s;
    }

    static std::wstring ExpandEnvironmentStrings (const std::wstring& s)
    {
        DWORD len = ::ExpandEnvironmentStrings (s.c_str(), NULL, 0);
        if (len == 0)
            return s;

        std::unique_ptr<TCHAR[]> buf(new TCHAR[len+1]);
        if (::ExpandEnvironmentStrings (s.c_str(), buf.get(), len) == 0)
            return s;

        return buf.get();
    }

    static std::string ToHexString( BYTE* pSrc, int nSrcLen );

    static bool FromHexString( const std::string& src, BYTE* pDest );

    static std::wstring ToHexWString( BYTE* pSrc, int nSrcLen );

    static bool FromHexString( const std::wstring& src, BYTE* pDest );
};
