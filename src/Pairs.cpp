// CryptSync - A folder sync tool with encryption

// Copyright (C) 2012-2014, 2016, 2019, 2021 - Stefan Kueng

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
#include "Pairs.h"
#include "Registry.h"
#include "StringUtils.h"
#include <algorithm>
#include <cassert>

void PairData::UpdateVec(std::wstring& s, std::vector<std::wstring>& v)
{
    std::transform(s.begin(), s.end(), s.begin(), ::towlower);
    stringtok(v, s, true);
}

bool PairData::IsCryptOnly(const std::wstring& s) const
{
    return MatchInVec(m_cryptOnlyVec, s);
}

bool PairData::IsCopyOnly(const std::wstring& s) const
{
    return MatchInVec(m_copyOnlyVec, s);
}

bool PairData::IsIgnored(const std::wstring& s) const
{
    return MatchInVec(m_noSyncVec, s);
}

bool PairData::MatchInVec(const std::vector<std::wstring>& v, const std::wstring& s)
{
    bool bMatched = false;
    if (v.empty())
        return false;
    std::wstring sCmp = s;
    std::transform(sCmp.begin(), sCmp.end(), sCmp.begin(), ::towlower);

    // first check if the whole path matches
    for (auto it = v.cbegin(); it != v.cend(); ++it)
    {
        if (wcswildcmp(it->c_str(), sCmp.c_str()))
        {
            return true;
        }
    }

    std::vector<std::wstring> pathelems;
    stringtok(pathelems, sCmp, true, L"\\");
    for (auto pe : pathelems)
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

CPairs::~CPairs()
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
        CRegStdString origPathReg(key);
        pd.m_origPath = origPathReg;
        if (pd.m_origPath.empty())
            break;

        swprintf_s(key, L"Software\\CryptSync\\SyncPairCrypt%d", p);
        CRegStdString cryptPathReg(key);
        pd.m_cryptPath = cryptPathReg;
        if (pd.m_cryptPath.empty())
            break;

        swprintf_s(key, L"Software\\CryptSync\\SyncPairPass%d", p);
        CRegStdString passwordReg(key);
        pd.m_password = passwordReg;
        if (pd.m_password.empty())
            break;
        pd.m_password = Decrypt(pd.m_password);

        swprintf_s(key, L"Software\\CryptSync\\SyncPairCryptOnly%d", p);
        CRegStdString cryptOnlyReg(key);
        pd.cryptOnly(cryptOnlyReg);

        swprintf_s(key, L"Software\\CryptSync\\SyncPairCopyOnly%d", p);
        CRegStdString copyOnlyReg(key);
        pd.copyOnly(copyOnlyReg);

        swprintf_s(key, L"Software\\CryptSync\\SyncPairNoSync%d", p);
        CRegStdString noSyncReg(key);
        pd.noSync(noSyncReg);

        swprintf_s(key, L"Software\\CryptSync\\SyncPairEncnames%d", p);
        CRegStdDWORD encNamesReg(key, TRUE);
        pd.m_encNames = !!static_cast<DWORD>(encNamesReg);

        swprintf_s(key, L"Software\\CryptSync\\SyncPairEncnamesNew%d", p);
        CRegStdDWORD encNamesNewReg(key, FALSE);
        pd.m_encNamesNew = !!static_cast<DWORD>(encNamesNewReg);

        swprintf_s(key, L"Software\\CryptSync\\SyncPairOneWay%d", p);
        CRegStdDWORD oneWayReg(key, static_cast<DWORD>(-1));
        if (static_cast<DWORD>(oneWayReg) != static_cast<DWORD>(-1))
            pd.m_syncDir = (!!static_cast<DWORD>(oneWayReg) ? SrcToDst : BothWays);
        else
        {
            swprintf_s(key, L"Software\\CryptSync\\SyncPairDir%d", p);
            CRegStdDWORD syncDirReg(key, BothWays);
            pd.m_syncDir = static_cast<SyncDir>(static_cast<DWORD>(syncDirReg));
        }
        oneWayReg.removeValue();

        swprintf_s(key, L"Software\\CryptSync\\SyncPair7zExt%d", p);
        CRegStdDWORD zExtReg(key, FALSE);
        pd.m_use7Z = !!static_cast<DWORD>(zExtReg);

        swprintf_s(key, L"Software\\CryptSync\\UseGPG%d", p);
        CRegStdDWORD zGpgReg(key, FALSE);
        pd.m_useGpg = !!static_cast<DWORD>(zGpgReg);

        swprintf_s(key, L"Software\\CryptSync\\SyncPairFAT%d", p);
        CRegStdDWORD fatReg(key, FALSE);
        pd.m_fat = !!static_cast<DWORD>(fatReg);

        swprintf_s(key, L"Software\\CryptSync\\SyncDeleted%d", p);
        CRegStdDWORD syncDelReg(key, TRUE);
        pd.m_syncDeleted = !!static_cast<DWORD>(syncDelReg);

        swprintf_s(key, L"Software\\CryptSync\\SyncPairCompressSize%d", p);
        CRegStdDWORD compressSizeReg(key, 100);
        pd.m_compressSize = static_cast<DWORD>(compressSizeReg);

        swprintf_s(key, L"Software\\CryptSync\\ResetOriginalArchiveAttribute%d", p);
        CRegStdDWORD resetOriginalArchiveAttrReg(key, FALSE);
        pd.m_ResetOriginalArchAttr = !!static_cast<DWORD>(resetOriginalArchiveAttrReg);

        swprintf_s(key, L"Software\\CryptSync\\SyncPairEnabled%d", p);
        CRegStdDWORD enabledReg(key, TRUE);
        pd.m_enabled = !!static_cast<DWORD>(enabledReg);

        if (std::find(cbegin(), cend(), pd) == cend())
            push_back(pd);
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
        // ReSharper disable CppEntityAssignedButNoRead
        CRegStdString origPathReg(key, L"", true);
        origPathReg = it->m_origPath;

        swprintf_s(key, L"Software\\CryptSync\\SyncPairCrypt%d", p);
        CRegStdString cryptPathReg(key, L"", true);
        cryptPathReg = it->m_cryptPath;

        swprintf_s(key, L"Software\\CryptSync\\SyncPairPass%d", p);
        CRegStdString passwordReg(key, L"", true);
        passwordReg = Encrypt(it->m_password);

        swprintf_s(key, L"Software\\CryptSync\\SyncPairCryptOnly%d", p);
        CRegStdString cryptOnlyReg(key, L"", true);
        cryptOnlyReg = it->cryptOnly();

        swprintf_s(key, L"Software\\CryptSync\\SyncPairCopyOnly%d", p);
        CRegStdString copyOnlyReg(key, L"", true);
        copyOnlyReg = it->copyOnly();

        swprintf_s(key, L"Software\\CryptSync\\SyncPairNoSync%d", p);
        CRegStdString noSyncReg(key, L"", true);
        noSyncReg = it->noSync();

        swprintf_s(key, L"Software\\CryptSync\\SyncPairEncnames%d", p);
        CRegStdDWORD encNamesReg(key, TRUE, true);
        encNamesReg = static_cast<DWORD>(it->m_encNames);

        swprintf_s(key, L"Software\\CryptSync\\SyncPairEncnamesNew%d", p);
        CRegStdDWORD encNamesNewReg(key, FALSE, true);
        encNamesNewReg = static_cast<DWORD>(it->m_encNamesNew);

        swprintf_s(key, L"Software\\CryptSync\\SyncPairDir%d", p);
        CRegStdDWORD syncDirReg(key, BothWays, true);
        syncDirReg = static_cast<DWORD>(it->m_syncDir);

        swprintf_s(key, L"Software\\CryptSync\\SyncPair7zExt%d", p);
        CRegStdDWORD zExtReg(key, FALSE, true);
        zExtReg = static_cast<DWORD>(it->m_use7Z);

        swprintf_s(key, L"Software\\CryptSync\\UseGPG%d", p);
        CRegStdDWORD zGpgReg(key, FALSE, true);
        zGpgReg = static_cast<DWORD>(it->m_useGpg);

        swprintf_s(key, L"Software\\CryptSync\\SyncPairFAT%d", p);
        CRegStdDWORD fatReg(key, FALSE, true);
        fatReg = static_cast<DWORD>(it->m_fat);

        swprintf_s(key, L"Software\\CryptSync\\SyncDeleted%d", p);
        CRegStdDWORD syncDelReg(key, TRUE, true);
        syncDelReg = static_cast<DWORD>(it->m_syncDeleted);

        swprintf_s(key, L"Software\\CryptSync\\SyncPairCompressSize%d", p);
        CRegStdDWORD compressSizeReg(key, 100, true);
        compressSizeReg = static_cast<DWORD>(it->m_compressSize);

        swprintf_s(key, L"Software\\CryptSync\\ResetOriginalArchiveAttribute%d", p);
        CRegStdDWORD resetOriginalArchiveAttrReg(key, FALSE, true);
        resetOriginalArchiveAttrReg = static_cast<DWORD>(it->m_ResetOriginalArchAttr);

        swprintf_s(key, L"Software\\CryptSync\\SyncPairEnabled%d", p);
        CRegStdDWORD enabledReg(key, TRUE, true);
        enabledReg = static_cast<DWORD>(it->m_enabled);
        // ReSharper restore CppEntityAssignedButNoRead

        ++p;
    }
    // delete all possible remaining registry entries
    while (p < 200)
    {
        WCHAR key[MAX_PATH];
        swprintf_s(key, L"Software\\CryptSync\\SyncPairOrig%d", p);
        CRegStdString origPathReg(key);
        origPathReg.removeValue();

        swprintf_s(key, L"Software\\CryptSync\\SyncPairCrypt%d", p);
        CRegStdString cryptPathReg(key);
        cryptPathReg.removeValue();

        swprintf_s(key, L"Software\\CryptSync\\SyncPairPass%d", p);
        CRegStdString passwordReg(key);
        passwordReg.removeValue();

        swprintf_s(key, L"Software\\CryptSync\\SyncPairCryptOnly%d", p);
        CRegStdString cryptOnlyReg(key);
        cryptOnlyReg.removeValue();

        swprintf_s(key, L"Software\\CryptSync\\SyncPairCopyOnly%d", p);
        CRegStdString copyOnlyReg(key);
        copyOnlyReg.removeValue();

        swprintf_s(key, L"Software\\CryptSync\\SyncPairNoSync%d", p);
        CRegStdString noSyncReg(key);
        noSyncReg.removeValue();

        swprintf_s(key, L"Software\\CryptSync\\SyncPairEncnames%d", p);
        CRegStdDWORD encNamesReg(key);
        encNamesReg.removeValue();

        swprintf_s(key, L"Software\\CryptSync\\SyncPairEncnamesNew%d", p);
        CRegStdDWORD encNamesNewReg(key);
        encNamesNewReg.removeValue();

        swprintf_s(key, L"Software\\CryptSync\\SyncPairDir%d", p);
        CRegStdDWORD syncDirReg(key);
        syncDirReg.removeValue();

        swprintf_s(key, L"Software\\CryptSync\\SyncPairOneWay%d", p);
        CRegStdDWORD oneWayReg(key);
        oneWayReg.removeValue();

        swprintf_s(key, L"Software\\CryptSync\\SyncPair7zExt%d", p);
        CRegStdDWORD zExtReg(key);
        zExtReg.removeValue();

        swprintf_s(key, L"Software\\CryptSync\\UseGPG%d", p);
        CRegStdDWORD zGpgReg(key);
        zGpgReg.removeValue();

        swprintf_s(key, L"Software\\CryptSync\\SyncDeleted%d", p);
        CRegStdDWORD syncDelReg(key);
        syncDelReg.removeValue();

        swprintf_s(key, L"Software\\CryptSync\\SyncPairFAT%d", p);
        CRegStdDWORD fatReg(key);
        fatReg.removeValue();

        swprintf_s(key, L"Software\\CryptSync\\SyncPairCompressSize%d", p);
        CRegStdDWORD compressSizeReg(key);
        compressSizeReg.removeValue();

        swprintf_s(key, L"Software\\CryptSync\\ResetOriginalArchiveAttribute%d", p);
        CRegStdDWORD resetOriginalArchiveAttrReg(key);
        resetOriginalArchiveAttrReg.removeValue();

        swprintf_s(key, L"Software\\CryptSync\\SyncPairEnabled%d", p);
        CRegStdDWORD enabledReg(key);
        enabledReg.removeValue();
        ++p;
    }
}

PairData::PairData(bool enabled, const std::wstring& orig, const std::wstring& crypt, const std::wstring& password, const std::wstring& cryptOnly, const std::wstring& copyOnly, const std::wstring& noSync, int compressSize, bool encryptNames, bool encryptNamesNew, SyncDir syncDir, bool use7ZExt, bool useGpg, bool fat, bool syncDeleted, bool ResetOriginalArchAttr)
{
    m_enabled   = enabled;
    m_origPath  = orig;
    m_cryptPath = crypt;
    m_password  = password;
    this->cryptOnly(cryptOnly);
    this->copyOnly(copyOnly);
    this->noSync(noSync);
    m_encNames              = encryptNames;
    m_encNamesNew           = encryptNamesNew;
    m_syncDir               = syncDir;
    m_use7Z                 = use7ZExt;
    m_useGpg                = useGpg;
    m_fat                   = fat;
    m_compressSize          = compressSize;
    m_syncDeleted           = syncDeleted;
    m_ResetOriginalArchAttr = ResetOriginalArchAttr;

    // make sure the paths are not root names but if root then root paths (i.e., ends with a backslash)
    if (*m_origPath.rbegin() == ':')
        m_origPath += '\\';
    if (*m_cryptPath.rbegin() == ':')
        m_cryptPath += '\\';
}
bool CPairs::AddPair(PairData& pd)
{
    if (std::find(cbegin(), cend(), pd) == cend())
    {
        push_back(pd);
        return true;
    }

    return false;
}

bool CPairs::AddPair(bool enabled, const std::wstring& orig, const std::wstring& crypt, const std::wstring& password, const std::wstring& cryptOnly, const std::wstring& copyOnly, const std::wstring& noSync, int compressSize, bool encryptNames, bool encryptNamesNew, SyncDir syncDir, bool use7ZExt, bool useGpg, bool fat, bool syncDeleted, bool ResetOriginalArchAttr)
{
    auto pd = PairData(enabled, orig, crypt, password, cryptOnly, copyOnly, noSync, compressSize, encryptNames, encryptNamesNew, syncDir, use7ZExt, useGpg, fat, syncDeleted, ResetOriginalArchAttr);

    return (AddPair(pd));
}

std::wstring CPairs::Decrypt(const std::wstring& pw) const
{
    DWORD dwLen = 0;

    if (CryptStringToBinary(pw.c_str(), static_cast<DWORD>(pw.size()), CRYPT_STRING_HEX, nullptr, &dwLen, nullptr, nullptr) == FALSE)
        return L"";

    auto strIn = std::make_unique<BYTE[]>(dwLen + 1);
    if (CryptStringToBinary(pw.c_str(), static_cast<DWORD>(pw.size()), CRYPT_STRING_HEX, strIn.get(), &dwLen, nullptr, nullptr) == FALSE)
        return L"";

    DATA_BLOB blobIn;
    blobIn.cbData = dwLen;
    blobIn.pbData = strIn.get();
    LPWSTR    descr;
    DATA_BLOB blobOut = {0};
    if (CryptUnprotectData(&blobIn, &descr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &blobOut) == FALSE)
        return L"";
    SecureZeroMemory(blobIn.pbData, blobIn.cbData);

    auto tempResult = std::make_unique<wchar_t[]>(blobOut.cbData + 1);
    wcsncpy_s(tempResult.get(), blobOut.cbData + 1, reinterpret_cast<const wchar_t*>(blobOut.pbData), blobOut.cbData / sizeof(wchar_t));
    SecureZeroMemory(blobOut.pbData, blobOut.cbData);
    LocalFree(blobOut.pbData);
    LocalFree(descr);

    std::wstring result = tempResult.get();

    return result;
}

std::wstring CPairs::Encrypt(const std::wstring& pw)
{
    DATA_BLOB    blobIn  = {0};
    DATA_BLOB    blobOut = {0};
    std::wstring result;

    blobIn.cbData = static_cast<DWORD>(pw.size()) * sizeof(wchar_t);
    blobIn.pbData = reinterpret_cast<BYTE*>(const_cast<wchar_t*>(pw.c_str()));
    if (CryptProtectData(&blobIn, L"CryptSyncRegPWs", nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &blobOut) == FALSE)
        return result;
    DWORD dwLen = 0;
    if (CryptBinaryToString(blobOut.pbData, blobOut.cbData, CRYPT_STRING_HEX, nullptr, &dwLen) == FALSE)
        return result;
    auto strOut = std::make_unique<wchar_t[]>(dwLen + 1);
    if (CryptBinaryToString(blobOut.pbData, blobOut.cbData, CRYPT_STRING_HEX, strOut.get(), &dwLen) == FALSE)
        return result;
    LocalFree(blobOut.pbData);

    result = strOut.get();

    return result;
}

#if defined(_DEBUG)
// Some test cases for these classes
[[maybe_unused]] static class CPairsTests
{
public:
    CPairsTests()
    {
        CPairs       p;
        std::wstring pw = p.Encrypt(L"password");
        pw              = p.Decrypt(pw);
        assert(pw == L"password");

        std::wstring pw2 = p.Encrypt(L"");
        pw2              = p.Decrypt(pw2);
        assert(pw2 == L"");
    }

} cPairsTestsobject;
#endif
