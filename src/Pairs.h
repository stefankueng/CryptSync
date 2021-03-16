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

#pragma once

#include <vector>
#include <string>

enum SyncDir
{
    BothWays,
    SrcToDst,
    DstToSrc
};

class PairData
{
public:
    PairData()
        : m_enabled(true)
        , m_encNames(false)
        , m_syncDir(BothWays)
        , m_use7Z(false)
        , m_useGpg(false)
        , m_fat(false)
        , m_compressSize(100)
        , m_syncDeleted(false)
    {
    }

    bool         m_enabled;
    std::wstring m_origPath;
    std::wstring m_cryptPath;
    std::wstring m_password;
    bool         m_encNames;
    SyncDir      m_syncDir;
    bool         m_use7Z;
    bool         m_useGpg;
    bool         m_fat;
    int          m_compressSize;
    bool         m_syncDeleted;
    std::wstring noSync() const { return m_noSync; }
    void         noSync(const std::wstring& c)
    {
        m_noSync = c;
        UpdateVec(m_noSync, m_noSyncVec);
    }
    bool         IsIgnored(const std::wstring& s) const;
    std::wstring cryptOnly() const { return m_cryptOnly; }
    void         cryptOnly(const std::wstring& c)
    {
        m_cryptOnly = c;
        UpdateVec(m_cryptOnly, m_cryptOnlyVec);
    }
    bool         IsCryptOnly(const std::wstring& s) const;
    std::wstring copyOnly() const { return m_copyOnly; }
    void         copyOnly(const std::wstring& c)
    {
        m_copyOnly = c;
        UpdateVec(m_copyOnly, m_copyOnlyVec);
    }
    bool IsCopyOnly(const std::wstring& s) const;

    friend bool operator<(const PairData& mk1, const PairData& mk2)
    {
        if (mk1.m_origPath != mk2.m_origPath)
            return mk1.m_origPath < mk2.m_origPath;

        return mk1.m_cryptPath < mk2.m_cryptPath;
    }
    friend bool operator==(const PairData& mk1, const PairData& mk2)
    {
        return ((mk1.m_origPath == mk2.m_origPath) && (mk1.m_cryptPath == mk2.m_cryptPath));
    }

private:
    static void UpdateVec(std::wstring& s, std::vector<std::wstring>& v);
    static bool MatchInVec(const std::vector<std::wstring>& v, const std::wstring& s);

    std::wstring              m_cryptOnly;
    std::vector<std::wstring> m_cryptOnlyVec;
    std::wstring              m_copyOnly;
    std::vector<std::wstring> m_copyOnlyVec;
    std::wstring              m_noSync;
    std::vector<std::wstring> m_noSyncVec;
};

typedef std::vector<PairData> PairVector;

/**
 * class to handle pairs of synced folders
 */
class CPairs : public PairVector
{
public:
    CPairs();
    ~CPairs();

    void SavePairs();
    bool AddPair(bool                enabled,
                 const std::wstring& orig,
                 const std::wstring& crypt,
                 const std::wstring& password,
                 const std::wstring& cryptOnly,
                 const std::wstring& copyOnly,
                 const std::wstring& noSync,
                 int                 compressSize,
                 bool                encryptNames,
                 SyncDir             syncDir,
                 bool                use7ZExt,
                 bool                useGpg,
                 bool                fat,
                 bool                syncDeleted);

protected:
    void InitPairList();

private:
    std::wstring        Decrypt(const std::wstring& pw) const;
    static std::wstring Encrypt(const std::wstring& pw);

    friend class CPairsTests;
};
