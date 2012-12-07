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

#include <vector>
#include <tuple>
#include <string>

typedef std::tuple<std::wstring, std::wstring, std::wstring, bool, bool> PairTuple;
typedef std::vector<PairTuple>                                           PairVector;


/**
 * class to handle pairs of synced folders
 */
class CPairs : public PairVector
{
public:
    CPairs();
    ~CPairs(void);

    void                    SavePairs();
    bool                    AddPair(const std::wstring& orig, const std::wstring& crypt, const std::wstring& password, bool encryptnames, bool oneway);
protected:
    void                    InitPairList();

private:
    std::wstring            Decrypt(const std::wstring& pw);
    std::wstring            Encrypt(const std::wstring& pw);

    friend class CPairsTests;
};
