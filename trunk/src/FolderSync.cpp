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
#include "StdAfx.h"
#include "FolderSync.h"
#include "DirFileEnum.h"
#include "DebugOutput.h"

#include <process.h>


CFolderSync::CFolderSync(void)
{
}


CFolderSync::~CFolderSync(void)
{
}

void CFolderSync::SyncFolders( const PairVector& pv )
{
    m_pairs = pv;
    unsigned int threadId = 0;
    _beginthreadex(NULL, 0, SyncFolderThreadEntry, this, 0, &threadId);
}

unsigned int CFolderSync::SyncFolderThreadEntry(void* pContext)
{
    ((CFolderSync*)pContext)->SyncFolderThread();
    return 0;
}

void CFolderSync::SyncFolderThread()
{
    for (auto it = m_pairs.cbegin(); it != m_pairs.cend(); ++it)
    {
        SyncFolder(*it);
    }
}

void CFolderSync::SyncFolder( const PairTuple& pt )
{
    std::map<std::wstring,FILETIME> origFileList  = GetFileList(std::get<0>(pt));
    std::map<std::wstring,FILETIME> cryptFileList = GetFileList(std::get<1>(pt));

    for (auto it = origFileList.cbegin(); it != origFileList.cend(); ++it)
    {
        auto cryptit = cryptFileList.find(it->first);
        if (cryptit == cryptFileList.end())
        {
            // file does not exist in the encrypted folder:
            // encrypt the file
            CTraceToOutputDebugString::Instance()(_T(__FUNCTION__) _T(": encrypt file %s to %s\n"), it->first.c_str(), std::get<1>(pt).c_str());
        }
        else
        {
            LONG cmp = CompareFileTime(&it->second, &cryptit->second);
            if (cmp < 0)
            {
                // original file is older than the encrypted file
                // decrypt the file
            }
            else if (cmp > 0)
            {
                // encrypted file is older than the original file
                // encrypt the file
            }
            else if (cmp == 0)
            {
                // files are identical (have the same last-write-time):
                // nothing to do.
                CTraceToOutputDebugString::Instance()(_T(__FUNCTION__) _T(": nothing to do for file %s\n"), it->first.c_str());
            }
        }
    }
    // now go through the encrypted file list and if there's a file that's not in the original file list,
    // decrypt it
    for (auto it = cryptFileList.cbegin(); it != cryptFileList.cend(); ++it)
    {
        auto origit = origFileList.find(it->first);
        if (origit == origit.end())
        {
            // file does not exist in the original folder:
            // decrypt the file
            CTraceToOutputDebugString::Instance()(_T(__FUNCTION__) _T(": decrypt file %s to %s\n"), it->first.c_str(), std::get<0>(pt).c_str());
        }
    }
}

std::map<std::wstring,FILETIME> CFolderSync::GetFileList( const std::wstring& path )
{
    CDirFileEnum enumerator(path);

    std::map<std::wstring,FILETIME> filelist;
    std::wstring filepath;
    bool isDir = false;
    while (enumerator.NextFile(filepath, &isDir, true))
    {
        if (isDir)
            continue;

        FILETIME ft = enumerator.GetLastWriteTime();
        if ((ft.dwLowDateTime == 0) && (ft.dwHighDateTime == 0))
            ft = enumerator.GetCreateTime();

        std::wstring relpath = filepath.substr(path.size()+1);

        filelist[relpath] = ft;
    }

    return filelist;
}

bool CFolderSync::EncryptFile( const std::wstring& orig, const std::wstring& crypt, const std::wstring& password )
{
    return true;
}

bool CFolderSync::DecryptFile( const std::wstring& orig, const std::wstring& crypt, const std::wstring& password )
{
    return true;
}
