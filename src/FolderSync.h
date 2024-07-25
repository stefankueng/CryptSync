// CryptSync - A folder sync tool with encryption

// Copyright (C) 2012-2016, 2018-2019, 2021, 2023-2024 - Stefan Kueng

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

    std::wstring fileRelPath; ///< real filename, possibly encrypted
    FILETIME     ft;
    bool         filenameEncrypted; ///< if the filename is encrypted
};

enum SyncOp
{
    None,
    Encrypt,
    Decrypt,
};

constexpr int ErrorNone      = 0;
constexpr int ErrorCancelled = 1;
constexpr int ErrorAccess    = 2;
constexpr int ErrorCrypt     = 4;
constexpr int ErrorCopy      = 8;

class CFolderSync
{
public:
    CFolderSync();
    ~CFolderSync();

    void                           SyncFolders(const PairVector& pv, HWND hWnd = nullptr);
    int                            SyncFoldersWait(const PairVector& pv, HWND hWnd = nullptr);
    bool                           SyncFile(const std::wstring& path);
    void                           SetPairs(const PairVector& pv);
    void                           Stop();
    std::map<std::wstring, SyncOp> GetFailures();
    std::set<std::wstring>         GetNotifyIgnores();
    size_t                         GetFailureCount();
    void                           SetTrayWnd(HWND hTray) { m_trayWnd = hTray; }
    void                           DecryptOnly(bool b) { m_decryptOnly = b; }
    bool                           IsRunning() const { return m_bRunning != 0; }

    // puclic only for tests
    static std::wstring            GetDecryptedFilename(const std::wstring& filename, const std::wstring& password, bool encryptName, bool newEncryption, bool use7Z, bool useGpg);
    static std::wstring            GetEncryptedFilename(const std::wstring& filename, const std::wstring& password, bool encryptName, bool newEncryption, bool use7Z, bool useGpg);

private:
    static unsigned int __stdcall SyncFolderThreadEntry(void* pContext);
    void                                       SyncFile(const std::wstring& plainPath, const PairData& pt);
    int                                        SyncFolderThread();
    int                                        SyncFolder(const PairData& pt);
    std::map<std::wstring, FileData, ci_lessW> GetFileList(bool orig, const std::wstring& path, const std::wstring& password, bool encnames, bool encnamesnew, bool use7Z, bool useGpg, DWORD& error) const;
    bool                                       EncryptFile(const std::wstring& orig, const std::wstring& crypt, const std::wstring& password, const FileData& fd, bool useGpg, bool noCompress, int compresssize, bool resetArchAttr);
    bool                                       DecryptFile(const std::wstring& orig, const std::wstring& crypt, const std::wstring& password, const FileData& fd, bool useGpg);
    static std::wstring                        GetFileTimeStringForLog(const FILETIME& ft);
    bool                                       RunGPG(LPWSTR cmdline, const std::wstring& cwd) const;
    // Would AdjustFileAttributes be a candidate for sktools?
    void                                       AdjustFileAttributes(const std::wstring& orig, DWORD dwFileAttributesToClear, DWORD dwFileAttributesToSet) const;
    static bool                                DeletePathToTrash(const std::wstring& path);

    CReaderWriterLock                          m_guard;
    CReaderWriterLock                          m_failureGuard;
    CReaderWriterLock                          m_notingGuard;
    PairVector                                 m_pairs;
    std::wstring                               m_gnuPg;
    HWND                                       m_parentWnd;
    HWND                                       m_trayWnd;
    CProgressDlg*                              m_pProgDlg;
    DWORD                                      m_progress;
    DWORD                                      m_progressTotal;
    volatile LONG                              m_bRunning;
    CAutoGeneralHandle                         m_hThread;
    PairData                                   m_currentPath;
    std::map<std::wstring, SyncOp>             m_failures;
    std::set<std::wstring>                     m_notifyIgnores;
    bool                                       m_decryptOnly;
};
