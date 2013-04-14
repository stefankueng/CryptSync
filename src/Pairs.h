// CryptSync - A folder sync tool with encryption

// Copyright (C) 2012-2013 - Stefan Kueng

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
#include <tuple>
#include <string>

class PairData
{
public:
    PairData()
        : encnames(false)
        , oneway(false)
        , use7z(false)
    {
    }

    std::wstring            origpath;
    std::wstring            cryptpath;
    std::wstring            password;
    bool                    encnames;
    bool                    oneway;
    bool                    use7z;
    std::wstring            copyonly() const { return m_copyonly; }
    void                    copyonly(const std::wstring& c) { m_copyonly = c; UpdateVec(); }
    bool                    IsCopyOnly(const std::wstring& s) const;

    friend bool operator<(const PairData& mk1, const PairData& mk2)
    {
        if (mk1.origpath != mk2.origpath )
            return mk1.origpath < mk2.origpath;

        return mk1.cryptpath < mk2.cryptpath;
    }
    friend bool operator==(const PairData& mk1, const PairData& mk2)
    {
        return ((mk1.origpath==mk2.origpath)&&(mk1.cryptpath==mk2.cryptpath));
    }
private:
    void                        UpdateVec();

    std::wstring                m_copyonly;
    std::vector<std::wstring>   m_copyonlyvec;
};

typedef std::vector<PairData>   PairVector;

/**
 * class to handle pairs of synced folders
 */
class CPairs : public PairVector
{
public:
    CPairs();
    ~CPairs(void);

    void                    SavePairs();
    bool                    AddPair(const std::wstring& orig,
                                    const std::wstring& crypt,
                                    const std::wstring& password,
                                    const std::wstring& copyonly,
                                    bool encryptnames,
                                    bool oneway,
                                    bool use7zext);
protected:
    void                    InitPairList();

private:
    std::wstring            Decrypt(const std::wstring& pw);
    std::wstring            Encrypt(const std::wstring& pw);

    friend class CPairsTests;
};
