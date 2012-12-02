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
#include <assert.h>


#pragma comment(lib, "Crypt32.lib")

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

        swprintf_s(key, L"Software\\CryptSync\\SyncPairPass%d", p);
        CRegStdString passwordreg(key);
        std::wstring password = passwordreg;
        if (password.empty())
            break;
        password = Decrypt(password);

        swprintf_s(key, L"Software\\CryptSync\\SyncPairEncnames%d", p);
        CRegStdDWORD encnamesreg(key, TRUE);
        bool encnames = !!(DWORD)encnamesreg;

        swprintf_s(key, L"Software\\CryptSync\\SyncPairOneWay%d", p);
        CRegStdDWORD onewayreg(key, TRUE);
        bool oneway = !!(DWORD)onewayreg;

        auto t = std::make_tuple(origpath, cryptpath, password, encnames, oneway);
        if (std::find(cbegin(), cend(), t) == cend())
            push_back(t);
        ++p;
    }
    std::sort(begin(), end());
}

void CPairs::SavePairs()
{
    std::sort(begin(), end());
    int p = 0;
    for (auto it = cbegin(); (it != cend()) && (p < 200); ++it)
    {
        WCHAR key[MAX_PATH];
        swprintf_s(key, L"Software\\CryptSync\\SyncPairOrig%d", p);
        CRegStdString origpathreg(key, L"", true);
        origpathreg = std::get<0>(*it);

        swprintf_s(key, L"Software\\CryptSync\\SyncPairCrypt%d", p);
        CRegStdString cryptpathreg(key, L"", true);
        cryptpathreg = std::get<1>(*it);

        swprintf_s(key, L"Software\\CryptSync\\SyncPairPass%d", p);
        CRegStdString passwordreg(key, L"", true);
        passwordreg = Encrypt(std::get<2>(*it));

        swprintf_s(key, L"Software\\CryptSync\\SyncPairEncnames%d", p);
        CRegStdDWORD encnamesreg(key, TRUE, true);
        encnamesreg = (DWORD)std::get<3>(*it);

        swprintf_s(key, L"Software\\CryptSync\\SyncPairOneWay%d", p);
        CRegStdDWORD onewayreg(key, TRUE, true);
        onewayreg = (DWORD)std::get<4>(*it);

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

        swprintf_s(key, L"Software\\CryptSync\\SyncPairPass%d", p);
        CRegStdString passwordreg(key);
        passwordreg.removeValue();

        swprintf_s(key, L"Software\\CryptSync\\SyncPairEncnames%d", p);
        CRegStdDWORD encnamesreg(key);
        encnamesreg.removeValue();

        swprintf_s(key, L"Software\\CryptSync\\SyncPairOneWay%d", p);
        CRegStdDWORD onewayreg(key);
        onewayreg.removeValue();
        ++p;
    }
}

bool CPairs::AddPair( const std::wstring& orig, const std::wstring& crypt, const std::wstring& password, bool encryptnames, bool oneway )
{
    auto t = std::make_tuple(orig, crypt, password, encryptnames, oneway);
    if (std::find(cbegin(), cend(), t) == cend())
    {
        push_back(t);
        std::sort(begin(), end());
        return true;
    }

    return false;
}

std::wstring CPairs::Decrypt( const std::wstring& pw )
{
    DWORD dwLen = 0;
    if (CryptStringToBinary(pw.c_str(), (DWORD)pw.size(), CRYPT_STRING_HEX, NULL, &dwLen, NULL, NULL)==FALSE)
        return L"";

    std::unique_ptr<BYTE[]> strIn(new BYTE[dwLen + 1]);
    if (CryptStringToBinary(pw.c_str(), (DWORD)pw.size(), CRYPT_STRING_HEX, strIn.get(), &dwLen, NULL, NULL)==FALSE)
        return L"";

    DATA_BLOB blobin;
    blobin.cbData = dwLen;
    blobin.pbData = strIn.get();
    LPWSTR descr;
    DATA_BLOB blobout = {0};
    if (CryptUnprotectData(&blobin, &descr, NULL, NULL, NULL, CRYPTPROTECT_UI_FORBIDDEN, &blobout)==FALSE)
        return L"";
    SecureZeroMemory(blobin.pbData, blobin.cbData);

    wchar_t * result = new wchar_t[blobout.cbData+1];
    wcsncpy_s(result, blobout.cbData+1, (const wchar_t*)blobout.pbData, blobout.cbData/sizeof(wchar_t));
    SecureZeroMemory(blobout.pbData, blobout.cbData);
    LocalFree(blobout.pbData);
    LocalFree(descr);
    return result;
}

std::wstring CPairs::Encrypt( const std::wstring& pw )
{
    DATA_BLOB blobin = {0};
    DATA_BLOB blobout = {0};
    std::wstring result;

    blobin.cbData = (DWORD)pw.size()*sizeof(wchar_t);
    blobin.pbData = (BYTE*)pw.c_str();
    if (CryptProtectData(&blobin, L"CryptSyncRegPWs", NULL, NULL, NULL, CRYPTPROTECT_UI_FORBIDDEN, &blobout)==FALSE)
        return result;
    DWORD dwLen = 0;
    if (CryptBinaryToString(blobout.pbData, blobout.cbData, CRYPT_STRING_HEX, NULL, &dwLen)==FALSE)
        return result;
    std::unique_ptr<wchar_t[]> strOut(new wchar_t[dwLen + 1]);
    if (CryptBinaryToString(blobout.pbData, blobout.cbData, CRYPT_STRING_HEX, strOut.get(), &dwLen)==FALSE)
        return result;
    LocalFree(blobout.pbData);

    result = strOut.get();

    return result;
}


#if defined(_DEBUG)
// Some test cases for these classes
static class CPairsTests
{
public:
    CPairsTests()
    {
        CPairs p;
        std::wstring pw = p.Encrypt(L"password");
        pw = p.Decrypt(pw);
        assert(pw == L"password");

        std::wstring pw2 = p.Encrypt(L"");
        pw2 = p.Decrypt(pw2);
        assert(pw2 == L"");
    }


} CPairsTestsobject;
#endif
