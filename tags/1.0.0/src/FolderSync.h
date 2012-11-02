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
#include "ReaderWriterLock.h"
#include "ProgressDlg.h"
#include "SmartHandle.h"

#include <string>
#include <set>
#include <map>

class FileData
{
public:
    FileData()
        : filenameEncrypted(false)
    {
        ft.dwHighDateTime = 0;
        ft.dwLowDateTime  = 0;
    }
    ~FileData()
    {

    }

    std::wstring    filename;               ///< real filename, possibly encrypted
    FILETIME        ft;
    bool            filenameEncrypted;      ///< if the filename is encrypted
};

class CFolderSync
{
public:
    CFolderSync(void);
    ~CFolderSync(void);

    void SyncFolders(const PairVector& pv, HWND hWnd = NULL);
    void SyncFile(const std::wstring& path);
    void Stop();

private:
    static unsigned int __stdcall   SyncFolderThreadEntry(void* pContext);
    void                            SyncFile(const std::wstring& path, const PairTuple& pt);
    void                            SyncFolderThread();
    void                            SyncFolder(const PairTuple& pt);
    std::map<std::wstring,FileData> GetFileList(const std::wstring& path, const std::wstring& password, bool encnames) const;
    bool                            EncryptFile(const std::wstring& orig, const std::wstring& crypt, const std::wstring& password, const FileData& fd);
    bool                            DecryptFile(const std::wstring& orig, const std::wstring& crypt, const std::wstring& password, const FileData& fd);
    std::wstring                    GetDecryptedFilename(const std::wstring& filename, const std::wstring& password, bool encryptname) const;
    std::wstring                    GetEncryptedFilename(const std::wstring& filename, const std::wstring& password, bool encryptname) const;

    bool                            Run7Zip(LPWSTR cmdline, const std::wstring& cwd) const;

    CReaderWriterLock               m_guard;
    PairVector                      m_pairs;
    std::wstring                    m_sevenzip;
    HWND                            m_parentWnd;
    CProgressDlg *                  m_pProgDlg;
    DWORD                           m_progress;
    DWORD                           m_progressTotal;
    volatile LONG                   m_bRunning;
    CAutoGeneralHandle              m_hThread;
    PairTuple                       m_currentPath;
};

