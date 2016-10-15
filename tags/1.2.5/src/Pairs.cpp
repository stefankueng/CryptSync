// CryptSync - A folder sync tool with encryption

// Copyright (C) 2012-2014, 2016 - Stefan Kueng

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
#include "StringUtils.h"
#include <algorithm>
#include <assert.h>
#include <cctype>



void PairData::UpdateVec(std::wstring& s, std::vector<std::wstring>& v)
{
    std::transform(s.begin(), s.end(), s.begin(), std::tolower);
    stringtok(v, s, true);
}

bool PairData::IsCopyOnly( const std::wstring& s ) const
{
    return MatchInVec(m_copyonlyvec, s);
}

bool PairData::IsIgnored( const std::wstring& s ) const
{
    return MatchInVec(m_nosyncvec, s);
}

bool PairData::MatchInVec( const std::vector<std::wstring>& v, const std::wstring& s )
{
    bool bMatched = false;
    if (v.empty())
        return false;
    std::wstring scmp = s;
    std::transform(scmp.begin(), scmp.end(), scmp.begin(), std::tolower);

    // first check if the whole path matches
    for (auto it = v.cbegin(); it != v.cend(); ++it)
    {
        if (wcswildcmp(it->c_str(), scmp.c_str()))
        {
            return true;
        }
    }

    std::vector<std::wstring> pathelems;
    stringtok(pathelems, scmp, true, L"\\");
    for (auto pe:pathelems)
    {
        for (auto it = v.cbegin(); it != v.cend(); ++it)
        {
            if (wcswildcmp(it->c_str(), pe.c_str()))
            {
                bMatched = true;
                break;
            }
        }
        if (bMatched)
            break;
    }
    return bMatched;
}


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
        PairData pd;

        WCHAR key[MAX_PATH];
        swprintf_s(key, L"Software\\CryptSync\\SyncPairOrig%d", p);
        CRegStdString origpathreg(key);
        pd.origpath = origpathreg;
        if (pd.origpath.empty())
            break;

        swprintf_s(key, L"Software\\CryptSync\\SyncPairCrypt%d", p);
        CRegStdString cryptpathreg(key);
        pd.cryptpath = cryptpathreg;
        if (pd.cryptpath.empty())
            break;

        swprintf_s(key, L"Software\\CryptSync\\SyncPairPass%d", p);
        CRegStdString passwordreg(key);
        pd.password = passwordreg;
        if (pd.password.empty())
            break;
        pd.password = Decrypt(pd.password);

        swprintf_s(key, L"Software\\CryptSync\\SyncPairCopyOnly%d", p);
        CRegStdString copyonlyreg(key);
        pd.copyonly(copyonlyreg);

        swprintf_s(key, L"Software\\CryptSync\\SyncPairNoSync%d", p);
        CRegStdString nosyncreg(key);
        pd.nosync(nosyncreg);

        swprintf_s(key, L"Software\\CryptSync\\SyncPairEncnames%d", p);
        CRegStdDWORD encnamesreg(key, TRUE);
        pd.encnames = !!(DWORD)encnamesreg;

        swprintf_s(key, L"Software\\CryptSync\\SyncPairOneWay%d", p);
        CRegStdDWORD onewayreg(key, (DWORD)-1);
        if (DWORD(onewayreg) != (DWORD)-1)
            pd.syncDir == !!(DWORD)onewayreg ? SrcToDst : BothWays;
        else
        {
            swprintf_s(key, L"Software\\CryptSync\\SyncPairDir%d", p);
            CRegStdDWORD syncdirreg(key, BothWays);
            pd.syncDir = (SyncDir)(DWORD)syncdirreg;
        }
        onewayreg.removeValue();

        swprintf_s(key, L"Software\\CryptSync\\SyncPair7zExt%d", p);
        CRegStdDWORD zextreg(key, FALSE);
        pd.use7z = !!(DWORD)zextreg;

        swprintf_s(key, L"Software\\CryptSync\\UseGPG%d", p);
        CRegStdDWORD zGPGreg(key, FALSE);
        pd.useGPG = !!(DWORD)zGPGreg;

        swprintf_s(key, L"Software\\CryptSync\\SyncPairFAT%d", p);
        CRegStdDWORD fatreg(key, FALSE);
        pd.FAT = !!(DWORD)fatreg;

        if (std::find(cbegin(), cend(), pd) == cend())
            push_back(pd);
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
        origpathreg = it->origpath;

        swprintf_s(key, L"Software\\CryptSync\\SyncPairCrypt%d", p);
        CRegStdString cryptpathreg(key, L"", true);
        cryptpathreg = it->cryptpath;

        swprintf_s(key, L"Software\\CryptSync\\SyncPairPass%d", p);
        CRegStdString passwordreg(key, L"", true);
        passwordreg = Encrypt(it->password);

        swprintf_s(key, L"Software\\CryptSync\\SyncPairCopyOnly%d", p);
        CRegStdString copyonlyreg(key, L"", true);
        copyonlyreg = it->copyonly();

        swprintf_s(key, L"Software\\CryptSync\\SyncPairNoSync%d", p);
        CRegStdString nosyncreg(key, L"", true);
        nosyncreg = it->nosync();

        swprintf_s(key, L"Software\\CryptSync\\SyncPairEncnames%d", p);
        CRegStdDWORD encnamesreg(key, TRUE, true);
        encnamesreg = (DWORD)it->encnames;

        swprintf_s(key, L"Software\\CryptSync\\SyncPairDir%d", p);
        CRegStdDWORD syncdirreg(key, BothWays, true);
        syncdirreg = (DWORD)it->syncDir;

        swprintf_s(key, L"Software\\CryptSync\\SyncPair7zExt%d", p);
        CRegStdDWORD zextreg(key, FALSE, true);
        zextreg = (DWORD)it->use7z;

        swprintf_s(key, L"Software\\CryptSync\\UseGPG%d", p);
        CRegStdDWORD zGPGreg(key, FALSE, true);
        zGPGreg = (DWORD)it->useGPG;

        swprintf_s(key, L"Software\\CryptSync\\SyncPairFAT%d", p);
        CRegStdDWORD fatreg(key, FALSE, true);
        fatreg = (DWORD)it->FAT;

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

        swprintf_s(key, L"Software\\CryptSync\\SyncPairCopyOnly%d", p);
        CRegStdString copyonlyreg(key);
        copyonlyreg.removeValue();

        swprintf_s(key, L"Software\\CryptSync\\SyncPairNoSync%d", p);
        CRegStdString nosyncreg(key);
        nosyncreg.removeValue();

        swprintf_s(key, L"Software\\CryptSync\\SyncPairEncnames%d", p);
        CRegStdDWORD encnamesreg(key);
        encnamesreg.removeValue();

        swprintf_s(key, L"Software\\CryptSync\\SyncPairOneWay%d", p);
        CRegStdDWORD onewayreg(key);
        onewayreg.removeValue();

        swprintf_s(key, L"Software\\CryptSync\\SyncPair7zExt%d", p);
        CRegStdDWORD zextreg(key);
        zextreg.removeValue();

        swprintf_s(key, L"Software\\CryptSync\\UseGPG%d", p);
        CRegStdDWORD zGPGreg(key);
        zGPGreg.removeValue();

        swprintf_s(key, L"Software\\CryptSync\\SyncPairFAT%d", p);
        CRegStdDWORD fatreg(key);
        fatreg.removeValue();
        ++p;
    }
}

bool CPairs::AddPair(const std::wstring& orig, const std::wstring& crypt, const std::wstring& password, const std::wstring& copyonly, const std::wstring& nosync, bool encryptnames, SyncDir syncDir, bool use7zext, bool useGPGe, bool fat)
{
    PairData pd;
    pd.origpath = orig;
    pd.cryptpath = crypt;
    pd.password = password;
    pd.copyonly(copyonly);
    pd.nosync(nosync);
    pd.encnames = encryptnames;
    pd.syncDir = syncDir;
    pd.use7z = use7zext;
    pd.useGPG = useGPGe;
    pd.FAT = fat;
    if (std::find(cbegin(), cend(), pd) == cend())
    {
        push_back(pd);
        std::sort(begin(), end());
        return true;
    }

    return false;
}

std::wstring CPairs::Decrypt( const std::wstring& pw )
{
    DWORD dwLen = 0;
    std::wstring result;

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

    std::unique_ptr<wchar_t[]> tempResult(new wchar_t[blobout.cbData + 1]);
    wcsncpy_s(tempResult.get(), blobout.cbData + 1, (const wchar_t*)blobout.pbData, blobout.cbData / sizeof(wchar_t));
    SecureZeroMemory(blobout.pbData, blobout.cbData);
    LocalFree(blobout.pbData);
    LocalFree(descr);

    result = tempResult.get();

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
