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
#include "Pairs.h"

#include <string>
#include <set>
#include <map>

class CFolderSync
{
public:
    CFolderSync(void);
    ~CFolderSync(void);

    void SyncFolders(const PairVector& pv);

private:
    static unsigned int __stdcall   SyncFolderThreadEntry(void* pContext);
    void                            SyncFolderThread();
    void                            SyncFolder(const PairTuple& pt);
    std::map<std::wstring,FILETIME> GetFileList(const std::wstring& path);
    bool                            EncryptFile(const std::wstring& orig, const std::wstring& crypt, const std::wstring& password);
    bool                            DecryptFile(const std::wstring& orig, const std::wstring& crypt, const std::wstring& password);

    PairVector                      m_pairs;
};

