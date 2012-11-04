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

#ifdef UNICODE
#define _tcswildcmp wcswildcmp
#else
#define _tcswildcmp strwildcmp
#endif

/**
 * \ingroup Utils
 * Performs a wild card compare of two strings.
 * \param wild the wild card string
 * \param string the string to compare the wild card to
 * \return TRUE if the wild card matches the string, 0 otherwise
 * \par example
 * \code
 * if (strwildcmp("bl?hblah.*", "bliblah.jpeg"))
 *  printf("success\n");
 * else
 *  printf("not found\n");
 * if (strwildcmp("bl?hblah.*", "blabblah.jpeg"))
 *  printf("success\n");
 * else
 *  printf("not found\n");
 * \endcode
 * The output of the above code would be:
 * \code
 * success
 * not found
 * \endcode
 */
int strwildcmp(const char * wild, const char * string);
int wcswildcmp(const wchar_t * wild, const wchar_t * string);


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
