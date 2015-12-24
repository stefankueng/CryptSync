// CryptSync - A folder sync tool with encryption

// Copyright (C) 2012-2015 - Stefan Kueng

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
#include "StringUtils.h"

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

    std::wstring    filerelpath;            ///< real filename, possibly encrypted
    FILETIME        ft;
    bool            filenameEncrypted;      ///< if the filename is encrypted
};

enum SyncOp
{
    None,
    Encrypt,
    Decrypt,
};


class CFolderSync
{
public:
    CFolderSync(void);
    ~CFolderSync(void);

    void                            SyncFolders(const PairVector& pv, HWND hWnd = NULL);
    void                            SyncFoldersWait(const PairVector& pv, HWND hWnd = NULL);
    void                            SyncFile(const std::wstring& path);
    void                            SetPairs(const PairVector& pv);
    void                            Stop();
    std::map<std::wstring, SyncOp>  GetFailures();
    std::set<std::wstring>          GetNotifyIgnores();
    size_t                          GetFailureCount();
    void                            SetTrayWnd(HWND hTray) { m_TrayWnd = hTray; }

private:
    static unsigned int __stdcall   SyncFolderThreadEntry(void* pContext);
    void                            SyncFile(const std::wstring& path, const PairData& pt);
    void                            SyncFolderThread();
    void                            SyncFolder(const PairData& pt);
    std::map<std::wstring, FileData, ci_lessW> GetFileList(bool orig, const std::wstring& path, const std::wstring& password, bool encnames, bool use7z, bool useGPG, DWORD & error) const;
    bool                            EncryptFile(const std::wstring& orig, const std::wstring& crypt, const std::wstring& password, const FileData& fd, bool useGPG);
    bool                            DecryptFile(const std::wstring& orig, const std::wstring& crypt, const std::wstring& password, const FileData& fd, bool useGPG);
    static std::wstring             GetDecryptedFilename(const std::wstring& filename, const std::wstring& password, bool encryptname, bool use7z, bool useGPG);
    static std::wstring             GetEncryptedFilename(const std::wstring& filename, const std::wstring& password, bool encryptname, bool use7z, bool useGPG);
    static std::wstring             GetFileTimeStringForLog(const FILETIME & ft);
    bool                            RunExtTool(LPWSTR cmdline, const std::wstring& cwd, bool useGPG) const;

    CReaderWriterLock               m_guard;
    CReaderWriterLock               m_failureguard;
    CReaderWriterLock               m_notignguard;
    PairVector                      m_pairs;
    std::wstring                    m_sevenzip;
    std::wstring                    m_GnuPG;
    HWND                            m_parentWnd;
    HWND                            m_TrayWnd;
    CProgressDlg *                  m_pProgDlg;
    DWORD                           m_progress;
    DWORD                           m_progressTotal;
    volatile LONG                   m_bRunning;
    CAutoGeneralHandle              m_hThread;
    PairData                        m_currentPath;
    std::map<std::wstring, SyncOp>  m_failures;
    std::set<std::wstring>          m_notifyignores;
};
