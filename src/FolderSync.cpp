// CryptSync - A folder sync tool with encryption

// Copyright (C) 2012-2016, 2018-2021, 2023-2024 - Stefan Kueng

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
#include "FolderSync.h"
#include "DirFileEnum.h"
#include "StringUtils.h"
#include "UnicodeUtils.h"
#include "PathUtils.h"
#include "Ignores.h"
#include "CreateProcessHelper.h"
#include "SmartHandle.h"
#include "DebugOutput.h"
#include "CircularLog.h"
#include "OnOutOfScope.h"
#include "COMPtrs.h"

#include <process.h>
#include <shlobj.h>
#include <cctype>
#include <algorithm>
#include <comdef.h>

#include "../base4k/base4k.h"
#include "../lzma/Wrapper-CPP/C7Zip.h"

CFolderSync::CFolderSync()
    : m_parentWnd(nullptr)
    , m_trayWnd(nullptr)
    , m_pProgDlg(nullptr)
    , m_progress(0)
    , m_progressTotal(1)
    , m_bRunning(FALSE)
    , m_decryptOnly(false)
{
    static const wchar_t *gnuPGInstallPaths[] = {
        L"%ProgramFiles%\\GNU\\GnuPG\\Pub\\gpg.exe",
#ifdef _WIN64
        L"%ProgramFiles(x86)%\\GNU\\GnuPG\\Pub\\gpg.exe",
        L"%ProgramFiles(x86)%\\GnuPG\\bin\\gpg.exe", // gpg (GnuPG) 2.4.5
#else
        L"%ProgramW6432%\\GNU\\GnuPG\\Pub\\gpg.exe",
#endif
        L"%ProgramFiles%\\GNU\\GnuPG\\gpg.exe", // try the old version 1 of gpg
#ifdef _WIN64
        L"%ProgramW6432%\\GNU\\GnuPG\\gpg.exe"
#else
        L"%ProgramW6432%\\GNU\\GnuPG\\gpg.exe"
#endif
    };

    bool bgnuPGFound = false;
    for (const auto gnuPGInstallPath : gnuPGInstallPaths)
    {
        m_gnuPg = CStringUtils::ExpandEnvironmentStrings(gnuPGInstallPath);
        if (PathFileExists(m_gnuPg.c_str()))
        {
            bgnuPGFound = true;
            break;
        }
    }
    if (!bgnuPGFound)
    {
        wchar_t buf[1024] = {};
        GetModuleFileName(nullptr, buf, 1024);
        std::wstring dir = buf;
        dir              = dir.substr(0, dir.find_last_of('\\'));

        m_gnuPg          = dir + L"\\gpg.exe";
    }
}

CFolderSync::~CFolderSync()
{
    // Thread not always stopped when instance is deleted. For example,
    // selecting the Exit context menu on the icon while the Options
    // dialog is displayed can result in crashes. Here is how:
    // DoModal() on the Options dialog returns as the dialog is closed by
    // Windows and the CFolderSync thread will *then* be
    // created, running in parallel to the main thread which is
    // deleting this instance. CPathWatcher::~CPathWatcher()
    // also calls its Stop method.
    Stop();
}

void CFolderSync::Stop()
{
    InterlockedExchange(&m_bRunning, FALSE);
    if (m_hThread)
    {
        WaitForSingleObject(m_hThread, INFINITE);
        m_hThread.CloseHandle();
    }
}

void CFolderSync::SetPairs(const PairVector& pv)
{
    CAutoWriteLock locker(m_guard);
    m_pairs = pv;
}

void CFolderSync::SyncFolders(const PairVector& pv, HWND hWnd)
{
    if (m_parentWnd != nullptr)
    {   // An interactive sync is in progress
        if (hWnd == nullptr)
            return;                  // skip this background sync request (interactive prioritized)
        assert(hWnd == m_parentWnd); // New interative sync request
    }
    else
    {
        if (m_bRunning && hWnd == nullptr)
        {
            // let current sync complete, if we stopped it
            // some folders might never sync (calling this method
            // faster than time it needs to complete
            return;
        }
        // no sync in progress or requesting a foreground sync, start this one
    }
    if (m_bRunning)
    {
        Stop();
    }
    CAutoWriteLock locker(m_guard);
    m_pairs               = pv;
    m_parentWnd           = hWnd;
    unsigned int threadId = 0;
    InterlockedExchange(&m_bRunning, TRUE);
    m_hThread = reinterpret_cast<HANDLE>(_beginthreadex(nullptr, 0, SyncFolderThreadEntry, this, 0, &threadId));
}

int CFolderSync::SyncFoldersWait(const PairVector& pv, HWND hWnd)
{
    CAutoWriteLock locker(m_guard);
    m_pairs     = pv;
    m_parentWnd = hWnd;
    InterlockedExchange(&m_bRunning, TRUE);
    return SyncFolderThread();
}

unsigned int CFolderSync::SyncFolderThreadEntry(void* pContext)
{
    static_cast<CFolderSync*>(pContext)->SyncFolderThread();
    _endthreadex(0);
    return 0;
}

int CFolderSync::SyncFolderThread()
{
    int        ret = ErrorNone;
    PairVector pv;
    {
        CAutoReadLock locker(m_guard);
        pv = m_pairs;
    }
    m_progress      = 0;
    m_progressTotal = 1;
    if (m_parentWnd)
    {
        CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        m_pProgDlg = new CProgressDlg();
        m_pProgDlg->SetTitle(L"Syncing folders");
        m_pProgDlg->SetLine(0, L"scanning...");
        m_pProgDlg->SetProgress(m_progress, m_progressTotal);
        m_pProgDlg->ShowModal(m_parentWnd);
    }
    {
        CAutoWriteLock locker(m_failureGuard);
        m_failures.clear();
    }
    for (auto it = pv.cbegin(); (it != pv.cend()) && m_bRunning; ++it)
    {
        {
            CAutoWriteLock locker(m_guard);
            m_currentPath = *it;
        }
        ret |= SyncFolder(*it);
        {
            CAutoWriteLock locker(m_guard);
            m_currentPath = PairData();
        }
    }
    if (m_pProgDlg)
    {
        m_pProgDlg->Stop();
        delete m_pProgDlg;
        m_pProgDlg = nullptr;
        CoUninitialize();
    }
    PostMessage(m_parentWnd, WM_THREADENDED, 0, 0);
    m_parentWnd = nullptr;
    InterlockedExchange(&m_bRunning, FALSE);
    return ret;
}

bool CFolderSync::SyncFile(const std::wstring& path)
{
    // check if the path notification comes from a folder that's
    // currently synced in the sync thread. If so, we "requeue" the
    // SyncFile since it is possible the sync thread may have already passed the
    // syncing of this file.
    {
        std::wstring s = m_currentPath.m_origPath;
        if (!s.empty())
        {
            if ((path.size() > s.size()) &&
                (_wcsicmp(s.c_str(), path.substr(0, s.size()).c_str()) == 0))
                return false;
        }
        s = m_currentPath.m_cryptPath;
        if (!s.empty())
        {
            if ((path.size() > s.size()) &&
                (_wcsicmp(s.c_str(), path.substr(0, s.size()).c_str()) == 0))
                return false;
        }
    }

    PairData      pt;
    CAutoReadLock locker(m_guard);
    for (auto it = m_pairs.cbegin(); it != m_pairs.cend(); ++it)
    {
        if (!it->m_enabled)
            continue;
        std::wstring s = it->m_origPath;
        if (path.size() > s.size())
        {
            if ((_wcsicmp(path.substr(0, s.size()).c_str(), s.c_str()) == 0) && (path[s.size()] == '\\'))
            {
                pt = *it;
                SyncFile(path, pt);
                continue;
            }
        }
        s = it->m_cryptPath;
        if (path.size() > s.size())
        {
            if ((_wcsicmp(path.substr(0, s.size()).c_str(), s.c_str()) == 0) && (path[s.size()] == '\\'))
            {
                pt = *it;
                SyncFile(path, pt);
                continue;
            }
        }
    }
    return true;
}

void CFolderSync::SyncFile(const std::wstring& plainPath, const PairData& pt)
{
    std::wstring orig  = pt.m_origPath;
    std::wstring crypt = pt.m_cryptPath;
    if (orig.empty() || crypt.empty())
        return;
    auto path = plainPath;
    if (pt.IsIgnored(path))
        return;
    if (!pt.m_enabled)
        return;

    const bool bCryptOnly = pt.IsCryptOnly(path);
    bool       bCopyOnly  = pt.IsCopyOnly(path);
    if ((orig.size() < path.size()) && (_wcsicmp(path.substr(0, orig.size()).c_str(), orig.c_str()) == 0) && ((path[orig.size()] == '\\') || (path[orig.size()] == '/')))
    {
        crypt = CPathUtils::Append(crypt, GetEncryptedFilename(path.substr(orig.size()), pt.m_password, pt.m_encNames, pt.m_encNamesNew, pt.m_use7Z, pt.m_useGpg));
        if (bCopyOnly)
        {
            if (!PathFileExists(crypt.c_str()))
                crypt = CPathUtils::Append(pt.m_cryptPath, path.substr(orig.size()));
            else
                bCopyOnly = false;
        }
        orig = path;
    }
    else
    {
        orig = CPathUtils::Append(orig, GetDecryptedFilename(path.substr(crypt.size()), pt.m_password, pt.m_encNames, pt.m_encNamesNew, pt.m_use7Z, pt.m_useGpg));
        if (bCopyOnly)
        {
            if (!PathFileExists(orig.c_str()))
            {
                auto origCopy = CPathUtils::Append(pt.m_origPath, path.substr(crypt.size()));
                if (PathFileExists(origCopy.c_str()))
                    orig = origCopy;
                else
                    bCopyOnly = false;
            }
        }
        crypt = path;
    }
    crypt                                   = CPathUtils::AdjustForMaxPath(crypt);
    orig                                    = CPathUtils::AdjustForMaxPath(orig);
    path                                    = CPathUtils::AdjustForMaxPath(plainPath);

    WIN32_FILE_ATTRIBUTE_DATA fDataOrig     = {};
    WIN32_FILE_ATTRIBUTE_DATA fDdataCrypt   = {};
    bool                      bOrigMissing  = false;
    bool                      bCryptMissing = false;
    if (!GetFileAttributesEx(orig.c_str(), GetFileExInfoStandard, &fDataOrig))
    {
        DWORD lastError = GetLastError();
        if (lastError == ERROR_ACCESS_DENIED)
            return;
        bOrigMissing = (lastError == ERROR_FILE_NOT_FOUND);
    }
    if (!GetFileAttributesEx(crypt.c_str(), GetFileExInfoStandard, &fDdataCrypt))
    {
        DWORD lastError = GetLastError();
        if (lastError == ERROR_ACCESS_DENIED)
            return;
        bCryptMissing = (lastError == ERROR_FILE_NOT_FOUND);
    }

    if ((fDataOrig.ftLastWriteTime.dwLowDateTime == 0) && (fDataOrig.ftLastWriteTime.dwHighDateTime == 0) &&
        (_wcsicmp(orig.c_str(), path.c_str()) == 0) && bOrigMissing)
    {
        if (pt.m_syncDeleted)
        {
            // original file got deleted.
            // delete the encrypted file
            {
                CAutoWriteLock nLocker(m_notingGuard);
                m_notifyIgnores.insert(crypt);
            }
            CTraceToOutputDebugString::Instance()(_T(__FUNCTION__) _T(": file %s does not exist, delete file %s\n"), orig.c_str(), crypt.c_str());
            CCircularLog::Instance()(_T("INFO:    file %s does not exist, delete file %s"), orig.c_str(), crypt.c_str());

            if (!DeletePathToTrash(crypt))
            {
                // in case the notification was for a folder that got removed,
                // the GetDecryptedFilename() call above added the .cryptsync extension which
                // folders don't have. Remove that extension and try deleting again.
                auto crypt2 = crypt.substr(0, crypt.find_last_of('.'));
                if (!DeletePathToTrash(crypt2))
                {
                    // could not delete file to the trashbin, so delete it directly
                    DeleteFile(crypt.c_str());
                    DeleteFile(crypt2.c_str());
                }
            }
            return;
        }
        else
        {
            CTraceToOutputDebugString::Instance()(_T(__FUNCTION__) _T(": file %s does not exist and sync deleted not set, skipping delete file %s\n"), orig.c_str(), crypt.c_str());
            CCircularLog::Instance()(_T("INFO:    file %s does not exist and sync deleted not set, skipping delete file %s"), orig.c_str(), crypt.c_str());
        }
    }

    else if ((fDdataCrypt.ftLastWriteTime.dwLowDateTime == 0) && (fDdataCrypt.ftLastWriteTime.dwHighDateTime == 0) &&
             (_wcsicmp(crypt.c_str(), path.c_str()) == 0) && bCryptMissing)
    {
        if (pt.m_syncDeleted)
        {
            // encrypted file got deleted.
            // delete the original file as well
            {
                CAutoWriteLock nLocker(m_notingGuard);
                m_notifyIgnores.insert(orig);
            }
            // check if there's an unencrypted file instead of an encrypted one in the encrypted folder
            if (!bCopyOnly)
            {
                std::wstring cryptnot = crypt.substr(0, crypt.find_last_of('.'));
                if (!GetFileAttributesEx(cryptnot.c_str(), GetFileExInfoStandard, &fDdataCrypt))
                {
                    DWORD lastError = GetLastError();
                    if (lastError == ERROR_ACCESS_DENIED)
                        return;
                    bCryptMissing = (lastError == ERROR_FILE_NOT_FOUND);
                }
                else
                    bCryptMissing = false;
            }

            if (bCryptMissing)
            {
                CTraceToOutputDebugString::Instance()(_T(__FUNCTION__) _T(": file %s does not exist, delete file %s\n"), crypt.c_str(), orig.c_str());
                CCircularLog::Instance()(_T("INFO:    file %s does not exist, delete file %s"), crypt.c_str(), orig.c_str());

                if (!DeletePathToTrash(orig))
                {
                    // could not delete file to the trashbin, so delete it directly
                    DeleteFile(orig.c_str());
                }
                return;
            }

            else
            {
                CTraceToOutputDebugString::Instance()(_T(__FUNCTION__) _T(": file %s does not exist and sync deleted not set, skipping delete file %s\n"), crypt.c_str(), orig.c_str());
                CCircularLog::Instance()(_T("INFO:    file %s does not exist and sync deleted not set, skipping delete file %s"), crypt.c_str(), orig.c_str());
            }
        }
    }

    if (fDataOrig.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        return;
    if (fDdataCrypt.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        return;
    LONG cmp = CompareFileTime(&fDataOrig.ftLastWriteTime, &fDdataCrypt.ftLastWriteTime);
    if (pt.m_fat)
    {
        // round up to two seconds accuracy
        ULONGLONG qwResult;
        qwResult = (static_cast<ULONGLONG>(fDataOrig.ftLastWriteTime.dwHighDateTime) << 32) + fDataOrig.ftLastWriteTime.dwLowDateTime;
        if (qwResult % 20000000UL)
        {
            qwResult += 20000000UL;
            qwResult /= 20000000UL;
            qwResult *= 20000000UL;
        }
        fDataOrig.ftLastWriteTime.dwLowDateTime  = static_cast<DWORD>(qwResult & 0xFFFFFFFF);
        fDataOrig.ftLastWriteTime.dwHighDateTime = static_cast<DWORD>(qwResult >> 32);

        ULONGLONG qwResult2                      = (static_cast<ULONGLONG>(fDdataCrypt.ftLastWriteTime.dwHighDateTime) << 32) + fDdataCrypt.ftLastWriteTime.dwLowDateTime;
        if (qwResult2 % 20000000UL)
        {
            qwResult2 += 20000000UL;
            qwResult2 /= 20000000UL;
            qwResult2 *= 20000000UL;
        }
        fDdataCrypt.ftLastWriteTime.dwLowDateTime  = static_cast<DWORD>(qwResult2 & 0xFFFFFFFF);
        fDdataCrypt.ftLastWriteTime.dwHighDateTime = static_cast<DWORD>(qwResult2 >> 32);
        cmp                                        = CompareFileTime(&fDataOrig.ftLastWriteTime, &fDdataCrypt.ftLastWriteTime);

        // if the difference is smaller than 4 seconds (twice the FAT limit),
        // then assume the times are equal.
        if (qwResult > qwResult2)
        {
            if ((qwResult - qwResult2) < 40000000UL)
                cmp = 0;
        }
        else
        {
            if ((qwResult2 - qwResult) < 40000000UL)
                cmp = 0;
        }
    }
    if (cmp < 0)
    {
        CCircularLog::Instance()(L"INFO:    original file is older: %s : %s, %s : %s",
                                 crypt.c_str(), GetFileTimeStringForLog(fDdataCrypt.ftLastWriteTime).c_str(),
                                 orig.c_str(), GetFileTimeStringForLog(fDataOrig.ftLastWriteTime).c_str());
        // original file is older than the encrypted file
        if ((pt.m_syncDir == BothWays) || (pt.m_syncDir == DstToSrc))
        {
            // decrypt the file
            FileData fd;
            fd.ft = fDdataCrypt.ftLastWriteTime;
            if (bCopyOnly)
            {
                CCircularLog::Instance()(_T("INFO:    copy file %s to %s"), crypt.c_str(), orig.c_str());
                if (!CopyFile(crypt.c_str(), orig.c_str(), FALSE))
                {
                    std::wstring targetFolder = orig.substr(0, orig.find_last_of('\\'));
                    CPathUtils::CreateRecursiveDirectory(targetFolder);
                    CopyFile(crypt.c_str(), orig.c_str(), FALSE);
                }
            }
            else
                DecryptFile(orig, crypt, pt.m_password, fd, pt.m_useGpg);
        }
    }
    else if (cmp > 0)
    {
        CCircularLog::Instance()(L"INFO:    encrypted file is older: %s : %s, %s : %s",
                                 orig.c_str(), GetFileTimeStringForLog(fDataOrig.ftLastWriteTime).c_str(),
                                 crypt.c_str(), GetFileTimeStringForLog(fDdataCrypt.ftLastWriteTime).c_str());
        // encrypted file is older than the original file
        if ((pt.m_syncDir == BothWays) || (pt.m_syncDir == SrcToDst))
        {
            // encrypt the file
            FileData fd;
            fd.ft = fDataOrig.ftLastWriteTime;
            if (bCopyOnly)
            {
                CCircularLog::Instance()(_T("INFO:    copy file %s to %s"), orig.c_str(), crypt.c_str());
                bool bCopyFileResult = CopyFile(orig.c_str(), crypt.c_str(), FALSE);
                if (!bCopyFileResult)
                {
                    std::wstring targetFolder = crypt.substr(0, crypt.find_last_of('\\'));
                    CPathUtils::CreateRecursiveDirectory(targetFolder);
                    bCopyFileResult = CopyFile(orig.c_str(), crypt.c_str(), FALSE);
                }
                if (bCopyFileResult && pt.m_ResetOriginalArchAttr)
                {
                    // Reset archive attribute on original file
                    AdjustFileAttributes(orig.c_str(), FILE_ATTRIBUTE_ARCHIVE, 0);
                }
            }
            else
                EncryptFile(orig, crypt, pt.m_password, fd, pt.m_useGpg, bCryptOnly, pt.m_compressSize, pt.m_ResetOriginalArchAttr);
        }
    }
    else if (cmp == 0)
    {
        // files are identical (have the same last-write-time):
        // nothing to copy. Check if we need to reset Archive attribute on source file
        if ((pt.m_syncDir == BothWays) || (pt.m_syncDir == SrcToDst))
        {
            if (pt.m_ResetOriginalArchAttr)
            {
                // Clear archive attibute
                AdjustFileAttributes(orig.c_str(), FILE_ATTRIBUTE_ARCHIVE, 0);
            }
        }
    }
}

int CFolderSync::SyncFolder(const PairData& pt)
{
    if (!pt.m_enabled)
        return ErrorNone;

    CCircularLog::Instance()(L"INFO:    syncing folder orig \"%s\" with crypt \"%s\"", pt.m_origPath.c_str(), pt.m_cryptPath.c_str());
    CCircularLog::Instance()(L"INFO:    settings: encrypt names: %s, use 7z: %s, use GPG: %s, use FAT workaround: %s, sync deleted: %s, reset archive attr: %s",
                             pt.m_encNames ? L"yes" : L"no",
                             pt.m_use7Z ? L"yes" : L"no",
                             pt.m_useGpg ? L"yes" : L"no",
                             pt.m_fat ? L"yes" : L"no",
                             pt.m_syncDeleted ? L"yes" : L"no",
                             pt.m_ResetOriginalArchAttr ? L"yes" : L"no");
    if (m_pProgDlg)
    {
        m_pProgDlg->SetLine(0, L"scanning...");
        m_pProgDlg->SetLine(2, L"");
        m_pProgDlg->SetProgress(m_progress, m_progressTotal);
    }
    if (m_trayWnd)
        PostMessage(m_trayWnd, WM_PROGRESS, m_progress, m_progressTotal);
    {
        CAutoFile hTest = CreateFile(CPathUtils::AdjustForMaxPath(pt.m_origPath).c_str(), GENERIC_READ, FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
        if (!hTest)
        {
            CCircularLog::Instance()(L"ERROR:   error accessing path \"%s\", skipped", pt.m_origPath.c_str());
            return ErrorAccess;
        }
    }
    {
        CAutoFile hTest = CreateFile(CPathUtils::AdjustForMaxPath(pt.m_cryptPath).c_str(), GENERIC_READ, FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
        if (!hTest)
        {
            CCircularLog::Instance()(L"ERROR:   error accessing path \"%s\", skipped", pt.m_cryptPath.c_str());
            return ErrorAccess;
        }
    }
    DWORD dwErr        = 0;
    auto  origFileList = GetFileList(true, pt.m_origPath, pt.m_password, pt.m_encNames, pt.m_encNamesNew, pt.m_use7Z, pt.m_useGpg, dwErr);

    if (dwErr)
    {
        CCircularLog::Instance()(L"ERROR:   error enumerating path \"%s\", skipped", pt.m_origPath.c_str());
        return ErrorAccess;
    }
    if (m_decryptOnly)
    {
        // to only decrypt the files to the source folder,
        // we clear the list of files in the source folder:
        // that will skip the encryption phase and go straight
        // to the decrypting phase.
        origFileList.clear();
    }

    auto cryptFileList = GetFileList(false, pt.m_cryptPath, pt.m_password, pt.m_encNames, pt.m_encNamesNew, pt.m_use7Z, pt.m_useGpg, dwErr);
    if (dwErr)
    {
        CCircularLog::Instance()(L"ERROR:   error enumerating path \"%s\", skipped", pt.m_cryptPath.c_str());
        return ErrorAccess;
    }

    int retVal = ErrorNone;

    m_progressTotal += static_cast<DWORD>(origFileList.size() + cryptFileList.size());

    auto lastSaveTicks = GetTickCount64();

    for (auto it = origFileList.cbegin(); (it != origFileList.cend()) && m_bRunning; ++it)
    {
        if (GetTickCount64() - lastSaveTicks > 60000)
        {
            CCircularLog::Instance().Save();
            lastSaveTicks = GetTickCount64();
        }
        if (m_trayWnd)
            PostMessage(m_trayWnd, WM_PROGRESS, m_progress, m_progressTotal);
        if (m_pProgDlg)
        {
            m_pProgDlg->SetLine(0, L"syncing files");
            m_pProgDlg->SetLine(2, it->first.c_str(), true);
            m_pProgDlg->SetProgress(m_progress, m_progressTotal);
            if (m_pProgDlg->HasUserCancelled())
            {
                if (m_trayWnd)
                    PostMessage(m_trayWnd, WM_PROGRESS, 0, 0);
                retVal |= ErrorCancelled;
                break;
            }
        }
        ++m_progress;

        if (CIgnores::Instance().IsIgnored(CPathUtils::Append(pt.m_origPath, it->first)))
            continue;
        if (pt.IsIgnored(CPathUtils::Append(pt.m_origPath, it->first)))
            continue;
        bool bCryptOnly = pt.IsCryptOnly(CPathUtils::Append(pt.m_origPath, it->first));
        bool bCopyOnly  = pt.IsCopyOnly(CPathUtils::Append(pt.m_origPath, it->first));
        auto cryptIt    = cryptFileList.find(it->first);
        if (cryptIt == cryptFileList.end())
        {
            // file does not exist in the encrypted folder:
            if ((pt.m_syncDir == BothWays) || (pt.m_syncDir == SrcToDst))
            {
                // encrypt the file
                CTraceToOutputDebugString::Instance()(_T(__FUNCTION__) _T(": file %s does not exist in encrypted folder\n"), it->first.c_str());
                if (bCopyOnly)
                {
                    std::wstring cryptPath = CPathUtils::AdjustForMaxPath(CPathUtils::Append(pt.m_cryptPath, it->first));
                    std::wstring origPath  = CPathUtils::AdjustForMaxPath(CPathUtils::Append(pt.m_origPath, it->first));
                    CCircularLog::Instance()(_T("INFO:    copy file %s to %s"), origPath.c_str(), cryptPath.c_str());
                    bool bCopyFileResult = CopyFile(origPath.c_str(), cryptPath.c_str(), FALSE);
                    if (!bCopyFileResult)
                    {
                        std::wstring targetFolder = cryptPath;
                        targetFolder              = targetFolder.substr(0, targetFolder.find_last_of('\\'));
                        CPathUtils::CreateRecursiveDirectory(targetFolder);
                        bCopyFileResult = CopyFile(origPath.c_str(), cryptPath.c_str(), FALSE);
                        if (!bCopyFileResult) // Original file did not use !, need to confirm with author
                            retVal |= ErrorCopy;
                    }
                    if (bCopyFileResult && pt.m_ResetOriginalArchAttr)
                    {
                        // Reset archive attribute on original file
                        AdjustFileAttributes(origPath.c_str(), FILE_ATTRIBUTE_ARCHIVE, 0);
                    }
                }
                else
                {
                    std::wstring cryptPath = CPathUtils::AdjustForMaxPath(CPathUtils::Append(pt.m_cryptPath, GetEncryptedFilename(it->first, pt.m_password, pt.m_encNames, pt.m_encNamesNew, pt.m_use7Z, pt.m_useGpg)));
                    std::wstring origPath  = CPathUtils::AdjustForMaxPath(CPathUtils::Append(pt.m_origPath, it->first));
                    if (!EncryptFile(origPath, cryptPath, pt.m_password, it->second, pt.m_useGpg, bCryptOnly, pt.m_compressSize, pt.m_ResetOriginalArchAttr))
                        retVal |= ErrorCrypt;
                }
            }
            else if (pt.m_syncDir == DstToSrc)
            {
                if (pt.m_syncDeleted)
                {
                    // remove the original file
                    CTraceToOutputDebugString::Instance()(_T(__FUNCTION__) _T(": counterpart of file %s does not exist in crypted folder, delete file\n"), it->first.c_str());
                    CCircularLog::Instance()(_T("INFO:    counterpart of file %s does not exist in crypted folder, delete file"), it->first.c_str());
                    std::wstring orig = CPathUtils::Append(pt.m_origPath, it->second.fileRelPath);
                    {
                        CAutoWriteLock nLocker(m_notingGuard);
                        m_notifyIgnores.insert(orig);
                    }
                    if (!DeletePathToTrash(orig))
                    {
                        // could not delete file to the trashbin, so delete it directly
                        DeleteFile(orig.c_str());
                    }
                }
                else
                {
                    CTraceToOutputDebugString::Instance()(_T(__FUNCTION__) _T(": counterpart of file %s does not exist in crypted folder and sync deleted not set, skipping delete file\n"), it->first.c_str());
                    CCircularLog::Instance()(_T("INFO:    counterpart of file %s does not exist in crypted folder and sync deleted not set, skipping delete file"), it->first.c_str());
                }
            }
        }
        else
        {
            LONG cmp = 0;
            if (pt.m_fat)
            {
                // round up to two seconds accuracy
                FILETIME ft1{};
                ft1.dwLowDateTime  = it->second.ft.dwLowDateTime;
                ft1.dwHighDateTime = it->second.ft.dwHighDateTime;
                FILETIME ft2{};
                ft2.dwLowDateTime  = cryptIt->second.ft.dwLowDateTime;
                ft2.dwHighDateTime = cryptIt->second.ft.dwHighDateTime;

                ULONGLONG qwResult;
                qwResult = (static_cast<ULONGLONG>(ft1.dwHighDateTime) << 32) + ft1.dwLowDateTime;
                if (qwResult % 20000000UL)
                {
                    qwResult += 20000000UL;
                    qwResult /= 20000000UL;
                    qwResult *= 20000000UL;
                }
                ft1.dwLowDateTime   = static_cast<DWORD>(qwResult & 0xFFFFFFFF);
                ft1.dwHighDateTime  = static_cast<DWORD>(qwResult >> 32);

                ULONGLONG qwResult2 = (static_cast<ULONGLONG>(ft2.dwHighDateTime) << 32) + ft2.dwLowDateTime;
                if (qwResult2 % 20000000UL)
                {
                    qwResult2 += 20000000UL;
                    qwResult2 /= 20000000UL;
                    qwResult2 *= 20000000UL;
                }
                ft2.dwLowDateTime  = static_cast<DWORD>(qwResult2 & 0xFFFFFFFF);
                ft2.dwHighDateTime = static_cast<DWORD>(qwResult2 >> 32);

                cmp                = CompareFileTime(&ft1, &ft2);
                // if the difference is smaller than 4 seconds (twice the FAT limit),
                // then assume the times are equal.
                if (qwResult > qwResult2)
                {
                    if ((qwResult - qwResult2) < 40000000UL)
                        cmp = 0;
                }
                else
                {
                    if ((qwResult2 - qwResult) < 40000000UL)
                        cmp = 0;
                }
            }
            else
                cmp = CompareFileTime(&it->second.ft, &cryptIt->second.ft);
            if (cmp < 0)
            {
                CCircularLog::Instance()(L"INFO:    original file is older: %s : %s, %s : %s",
                                         (pt.m_cryptPath + L"\\" + it->first).c_str(), GetFileTimeStringForLog(cryptIt->second.ft).c_str(),
                                         (pt.m_origPath + L"\\" + it->first).c_str(), GetFileTimeStringForLog(it->second.ft).c_str());
                // original file is older than the encrypted file
                if ((pt.m_syncDir == BothWays) || (pt.m_syncDir == DstToSrc))
                {
                    // decrypt the file
                    CTraceToOutputDebugString::Instance()(_T(__FUNCTION__) _T(": file %s is older than its encrypted partner\n"), it->first.c_str());
                    if (bCopyOnly)
                    {
                        std::wstring cryptPath = CPathUtils::AdjustForMaxPath(CPathUtils::Append(pt.m_cryptPath, it->first));
                        std::wstring origPath  = CPathUtils::AdjustForMaxPath(CPathUtils::Append(pt.m_origPath, it->first));
                        CCircularLog::Instance()(_T("INFO:    copy file %s to %s"), cryptPath.c_str(), origPath.c_str());
                        if (!CopyFile(cryptPath.c_str(), origPath.c_str(), FALSE))
                        {
                            std::wstring targetFolder = pt.m_origPath + L"\\" + it->first;
                            targetFolder              = targetFolder.substr(0, targetFolder.find_last_of('\\'));
                            CPathUtils::CreateRecursiveDirectory(targetFolder);
                            if (!CopyFile(cryptPath.c_str(), origPath.c_str(), FALSE))
                                retVal |= ErrorCopy;
                        }
                    }
                    else
                    {
                        std::wstring cryptPath = CPathUtils::AdjustForMaxPath(CPathUtils::Append(pt.m_cryptPath, GetEncryptedFilename(it->first, pt.m_password, pt.m_encNames, pt.m_encNamesNew, pt.m_use7Z, pt.m_useGpg)));
                        std::wstring origPath  = CPathUtils::AdjustForMaxPath(CPathUtils::Append(pt.m_origPath, it->first));
                        if (!DecryptFile(origPath, cryptPath, pt.m_password, cryptIt->second, pt.m_useGpg))
                            retVal |= ErrorCrypt;
                    }
                }
            }
            else if (cmp > 0)
            {
                CCircularLog::Instance()(L"INFO:    encrypted file is older: %s : %s, %s : %s",
                                         (pt.m_origPath + L"\\" + it->first).c_str(), GetFileTimeStringForLog(it->second.ft).c_str(),
                                         (pt.m_cryptPath + L"\\" + it->first).c_str(), GetFileTimeStringForLog(cryptIt->second.ft).c_str());
                // encrypted file is older than the original file
                if ((pt.m_syncDir == BothWays) || (pt.m_syncDir == SrcToDst))
                {
                    // encrypt the file
                    CTraceToOutputDebugString::Instance()(_T(__FUNCTION__) _T(": file %s is newer than its encrypted partner\n"), it->first.c_str());
                    if (bCopyOnly)
                    {
                        std::wstring cryptPath = CPathUtils::AdjustForMaxPath(CPathUtils::Append(pt.m_cryptPath, it->first));
                        std::wstring origPath  = CPathUtils::AdjustForMaxPath(CPathUtils::Append(pt.m_origPath, it->first));
                        CCircularLog::Instance()(_T("INFO:    copy file %s to %s"), origPath.c_str(), cryptPath.c_str());
                        bool bCopyFileResult = CopyFile(origPath.c_str(), cryptPath.c_str(), FALSE);
                        if (!bCopyFileResult)
                        {
                            std::wstring targetFolder = pt.m_cryptPath + L"\\" + it->first;
                            targetFolder              = targetFolder.substr(0, targetFolder.find_last_of('\\'));
                            CPathUtils::CreateRecursiveDirectory(targetFolder);
                            bCopyFileResult = CopyFile(origPath.c_str(), cryptPath.c_str(), FALSE);
                            if (!bCopyFileResult)
                                retVal |= ErrorCopy;
                        }
                        if (bCopyFileResult && pt.m_ResetOriginalArchAttr)
                        {
                            // Clear archive attibute
                            AdjustFileAttributes(origPath.c_str(), FILE_ATTRIBUTE_ARCHIVE, 0);
                        }
                    }
                    else
                    {
                        std::wstring cryptPath = CPathUtils::AdjustForMaxPath(CPathUtils::Append(pt.m_cryptPath, GetEncryptedFilename(it->first, pt.m_password, pt.m_encNames, pt.m_encNamesNew, pt.m_use7Z, pt.m_useGpg)));
                        std::wstring origPath  = CPathUtils::AdjustForMaxPath(CPathUtils::Append(pt.m_origPath, it->first));
                        if (!EncryptFile(origPath, cryptPath, pt.m_password, it->second, pt.m_useGpg, bCryptOnly, pt.m_compressSize, pt.m_ResetOriginalArchAttr))
                            retVal |= ErrorCrypt;
                    }
                }
            }
            else if (cmp == 0)
            {
                // files are identical (have the same last-write-time):
                // nothing to copy. Check if we need to reset Archive attribute on source file
                if ((pt.m_syncDir == BothWays) || (pt.m_syncDir == SrcToDst))
                {
                    if (pt.m_ResetOriginalArchAttr)
                    {
                        std::wstring origPath = CPathUtils::AdjustForMaxPath(CPathUtils::Append(pt.m_origPath, it->first));

                        // Clear archive attibute
                        AdjustFileAttributes(origPath.c_str(), FILE_ATTRIBUTE_ARCHIVE, 0);
                    }
                }
            }
        }
    }
    // now go through the encrypted file list and if there's a file that's not in the original file list,
    // decrypt it
    for (auto it = cryptFileList.cbegin(); (it != cryptFileList.cend()) && m_bRunning; ++it)
    {
        if (m_trayWnd)
            PostMessage(m_trayWnd, WM_PROGRESS, m_progress, m_progressTotal);
        if (m_pProgDlg)
        {
            m_pProgDlg->SetLine(0, L"syncing files");
            m_pProgDlg->SetLine(2, it->first.c_str(), true);
            m_pProgDlg->SetProgress(m_progress, m_progressTotal);
            if (m_pProgDlg->HasUserCancelled())
            {
                if (m_trayWnd)
                    PostMessage(m_trayWnd, WM_PROGRESS, 0, 0);
                retVal |= ErrorCancelled;
                break;
            }
        }
        ++m_progress;

        if (CIgnores::Instance().IsIgnored(CPathUtils::Append(pt.m_origPath, it->first)))
            continue;
        if (pt.IsIgnored(CPathUtils::Append(pt.m_origPath, it->first)))
            continue;
        bool bCopyOnly = pt.IsCopyOnly(CPathUtils::Append(pt.m_origPath, it->first));
        auto origit    = origFileList.find(it->first);
        if (origit == origFileList.end())
        {
            // file does not exist in the original folder:
            if ((pt.m_syncDir == SrcToDst) && !origFileList.empty())
            {
                if (pt.m_syncDeleted)
                {
                    // remove the encrypted file
                    CTraceToOutputDebugString::Instance()(_T(__FUNCTION__) _T(": counterpart of file %s does not exist in src folder, delete file\n"), it->first.c_str());
                    CCircularLog::Instance()(_T("INFO:    counterpart of file %s does not exist in src folder, delete file"), it->first.c_str());
                    std::wstring crypt = CPathUtils::Append(pt.m_cryptPath, it->second.fileRelPath);
                    {
                        CAutoWriteLock nlocker(m_notingGuard);
                        m_notifyIgnores.insert(crypt);
                    }
                    if (!DeletePathToTrash(crypt))
                    {
                        // could not delete file to the trashbin, so delete it directly
                        DeleteFile(crypt.c_str());
                    }
                }
                else
                {
                    CTraceToOutputDebugString::Instance()(_T(__FUNCTION__) _T(": counterpart of file %s does not exist in src folder and sync deleted not set, skipping delete file\n"), it->first.c_str());
                    CCircularLog::Instance()(_T("INFO:    counterpart of file %s does not exist in src folder and sync deleted not set, skipping delete file"), it->first.c_str());
                }
            }
            else if (bCopyOnly && (origFileList.empty() || (pt.m_syncDir == BothWays) || (pt.m_syncDir == DstToSrc)))
            {
                std::wstring cryptPath = CPathUtils::AdjustForMaxPath(CPathUtils::Append(pt.m_cryptPath, it->first));
                std::wstring origPath  = CPathUtils::AdjustForMaxPath(CPathUtils::Append(pt.m_origPath, it->first));
                CCircularLog::Instance()(_T("INFO:    copy file %s to %s"), cryptPath.c_str(), origPath.c_str());
                // copy the file
                if (!CopyFile(cryptPath.c_str(), origPath.c_str(), FALSE))
                {
                    std::wstring targetFolder = pt.m_origPath + L"\\" + it->first;
                    targetFolder              = targetFolder.substr(0, targetFolder.find_last_of('\\'));
                    CPathUtils::CreateRecursiveDirectory(targetFolder);
                    {
                        CAutoWriteLock nLocker(m_notingGuard);
                        m_notifyIgnores.insert(CPathUtils::Append(pt.m_origPath, it->first));
                    }
                    if (!CopyFile(cryptPath.c_str(), origPath.c_str(), FALSE))
                        retVal |= ErrorCopy;
                }
            }

            else if ((pt.m_syncDir == BothWays) || (pt.m_syncDir == DstToSrc)
                     /** Only restore files in original folder
                      * if syncing is both ways or encrypted to original direction.
                      * Otherwise assume the intention is file should not be restored
                      **/
                     || (origFileList.empty() && (pt.m_syncDir == BothWays || pt.m_syncDir == DstToSrc)))
            {
                // decrypt the file
                CTraceToOutputDebugString::Instance()(_T(__FUNCTION__) _T(": decrypt file %s to %s\n"), it->first.c_str(), pt.m_origPath.c_str());
                std::wstring cryptPath = CPathUtils::AdjustForMaxPath(CPathUtils::Append(pt.m_cryptPath, it->second.fileRelPath));
                std::wstring origPath  = CPathUtils::AdjustForMaxPath(CPathUtils::Append(pt.m_origPath, it->first));
                if (!DecryptFile(origPath, cryptPath, pt.m_password, it->second, pt.m_useGpg))
                {
                    retVal |= ErrorCrypt;
                    if (!it->second.filenameEncrypted)
                    {
                        {
                            CAutoWriteLock nlocker(m_notingGuard);
                            m_notifyIgnores.insert(cryptPath);
                        }
                        MoveFileEx(cryptPath.c_str(), origPath.c_str(), MOVEFILE_COPY_ALLOWED);
                    }
                }
            }
        }
    }
    if (m_trayWnd)
        PostMessage(m_trayWnd, WM_PROGRESS, 0, 0);
    CCircularLog::Instance()(L"INFO:    finished syncing folder orig \"%s\" with crypt \"%s\"", pt.m_origPath.c_str(), pt.m_cryptPath.c_str());
    CCircularLog::Instance().Save();
    return retVal;
}

std::map<std::wstring, FileData, ci_lessW> CFolderSync::GetFileList(bool orig, const std::wstring& path, const std::wstring& password, bool encnames, bool encnamesnew, bool use7Z, bool useGpg, DWORD& error) const
{
    error                 = 0;
    std::wstring enumpath = path;
    if ((enumpath.size() == 2) && (enumpath[1] == ':'))
        enumpath += L"\\";
    enumpath = CPathUtils::AdjustForMaxPath(enumpath);
    CDirFileEnum                               enumerator(enumpath);

    std::map<std::wstring, FileData, ci_lessW> fileList;
    std::wstring                               filePath;
    bool                                       isDir    = false;
    bool                                       bRecurse = true;
    while (enumerator.NextFile(filePath, &isDir, bRecurse))
    {
        if (m_pProgDlg && m_pProgDlg->HasUserCancelled())
            break;
        if (!m_bRunning)
            break;

        bRecurse = true;
        if (isDir)
        {
            if (CIgnores::Instance().IsIgnored(filePath))
                bRecurse = false; // don't recurse into ignored folders
            continue;
        }

        FileData fd;

        fd.ft = enumerator.GetLastWriteTime();
        if ((fd.ft.dwLowDateTime == 0) && (fd.ft.dwHighDateTime == 0))
            fd.ft = enumerator.GetCreateTime();

        std::wstring relPath = filePath;
        if (enumpath.size() < filePath.size())
        {
            if (*enumpath.rbegin() == '\\')
                relPath = filePath.substr(enumpath.size());
            else
                relPath = filePath.substr(enumpath.size() + 1);
        }
        fd.fileRelPath                = relPath;

        std::wstring decryptedRelPath = relPath;
        if (!orig)
            decryptedRelPath = GetDecryptedFilename(relPath, password, encnames, encnamesnew, use7Z, useGpg);
        fd.filenameEncrypted = (_wcsicmp(decryptedRelPath.c_str(), fd.fileRelPath.c_str()) != 0);
        if (fd.filenameEncrypted)
        {
            if (use7Z && !orig)
            {
                // if we use .7z as the file extension and the user tries to sync her/his own .7z files,
                // we have to detect that here
                auto lastDotPos = filePath.rfind('.');
                if (lastDotPos != std::wstring::npos)
                {
                    if (_wcsicmp(filePath.substr(lastDotPos + 1).c_str(), L"7z") == 0)
                    {
                        auto preLastDotPos = filePath.rfind('.', lastDotPos - 1);
                        if (preLastDotPos != std::wstring::npos)
                            relPath = decryptedRelPath;
                        else if (encnames && decryptedRelPath != relPath)
                            relPath = decryptedRelPath;
                        else if (!orig)
                            relPath = decryptedRelPath; // orig file with no extension
                    }
                    else
                        relPath = decryptedRelPath;
                }
                else
                    relPath = decryptedRelPath;
            }
            else
                relPath = decryptedRelPath;
        }

        fileList[relPath] = fd;
        if (error == 0)
            error = enumerator.GetError();
    }
    return fileList;
}

bool CFolderSync::EncryptFile(const std::wstring& orig, const std::wstring& crypt, const std::wstring& password, const FileData& fd, bool useGpg, bool noCompress, int compresssize, bool resetArchAttr)
{
    CTraceToOutputDebugString::Instance()(_T(__FUNCTION__) _T(": encrypt file %s to %s\n"), orig.c_str(), crypt.c_str());
    CCircularLog::Instance()(_T("INFO:    encrypt file %s to %s"), orig.c_str(), crypt.c_str());

    size_t slashpos = crypt.find_last_of('\\');
    if (slashpos == std::string::npos)
        return false;
    std::wstring targetFolder = crypt.substr(0, slashpos);
    std::wstring cryptName    = crypt.substr(slashpos + 1);

    int          compression  = noCompress ? 0 : 9;
    if (!noCompress)
    {
        // try to open the source file in read mode:
        // if we can't open the file, then it is locked and 7-zip would fail.
        // But when 7-zip fails, it destroys a possible already existing encrypted file instead of
        // just leaving it as it is. So by first checking if the source file
        // can be read, we reduce the chances of 7-zip destroying the target file.
        CAutoFile hFile = CreateFile(orig.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, 0, nullptr);
        if (!hFile.IsValid())
            return false;
        LARGE_INTEGER fileSize = {};
        GetFileSizeEx(hFile, &fileSize);
        hFile.CloseHandle();

        if (fileSize.QuadPart > (compresssize * 1024LL * 1024LL))
            compression = 0; // turn off compression for files bigger than compresssize MB
    }

    if (!useGpg || password.empty())
    {
        if (password.empty())
            CCircularLog::Instance()(_T("ERROR:   password is blank - NOT secure - force 7z not GPG"), crypt.c_str());
        auto progressFunc = [&](UInt64, UInt64, const std::wstring&) {
            if (m_pProgDlg && m_pProgDlg->HasUserCancelled())
                return E_ABORT;
            return S_OK;
        };

        std::wstring encryptTmpFile = CPathUtils::GetTempFilePath();
        C7Zip        compressor;
        compressor.SetPassword(password);
        compressor.SetArchivePath(encryptTmpFile);
        compressor.SetCompressionFormat(CompressionFormat::SevenZip, compression);
        compressor.SetCallback(progressFunc);
        if (compressor.AddPath(orig))
        {
            CPathUtils::CreateRecursiveDirectory(targetFolder);
            if (MoveFileEx(encryptTmpFile.c_str(), (targetFolder + L"\\" + cryptName).c_str(), MOVEFILE_COPY_ALLOWED | MOVEFILE_REPLACE_EXISTING))
            {
                DeleteFile(encryptTmpFile.c_str());

                // set content not indexed and the file timestamp
                AdjustFileAttributes(targetFolder + L"\\" + cryptName.c_str(), 0, FILE_ATTRIBUTE_NOT_CONTENT_INDEXED);

                if (resetArchAttr)
                {
                    // Reset archive attribute on original file
                    AdjustFileAttributes(orig.c_str(), FILE_ATTRIBUTE_ARCHIVE, 0);
                }

                // Do equivalent of Z-zip's -stl option and set archive time based on archive's file timestamp
                // This is required to ensure future sync operations work (based on source / encrypted file's last-modified date)
                int  retry = 5;
                bool bRet  = true;
                do
                {
                    if (m_pProgDlg && m_pProgDlg->HasUserCancelled())
                        break;
                    CAutoFile hFileCrypt = CreateFile(crypt.c_str(), GENERIC_WRITE | GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
                    if (hFileCrypt.IsValid())
                    {
                        bRet = !!SetFileTime(hFileCrypt, nullptr, nullptr, &fd.ft);
                    }
                    else
                        bRet = false;
                    if (!bRet)
                        Sleep(200);
                } while (!bRet && (retry-- > 0));
                if (!bRet) // Should archive file be erased in this case (future sync will be unreliable due to incorrect date)?
                    CCircularLog::Instance()(_T("INFO:    failed to set file time on %s"), crypt.c_str());
                CAutoWriteLock locker(m_failureGuard);
                m_failures.erase(orig);
                return true;
            }
            _com_error comError(::GetLastError());
            LPCTSTR    comErrorText = comError.ErrorMessage();

            CCircularLog::Instance()(L"ERROR:   error moving temporary encrypted file \"%s\" to \"%s\" (%s)", encryptTmpFile.c_str(), crypt.c_str(), comErrorText);
            DeleteFile(encryptTmpFile.c_str());
            return false;
        }
        else
        {
            // If encrypting failed, remove the leftover file
            DeleteFile(encryptTmpFile.c_str());
            CAutoWriteLock locker(m_failureGuard);
            m_failures[orig] = Encrypt;
            CCircularLog::Instance()(L"ERROR:   Failed to encrypt file \"%s\" to \"%s\"", orig.c_str(), crypt.c_str());
            return false;
        }
    }

    size_t bufLen     = orig.size() + crypt.size() + password.size() + 1000;
    auto   cmdlineBuf = std::make_unique<wchar_t[]>(bufLen);

    swprintf_s(cmdlineBuf.get(), bufLen, L"\"%s\" --batch --yes -c -a --passphrase \"%s\" -o \"%s\" \"%s\" ", m_gnuPg.c_str(), password.c_str(), crypt.c_str(), orig.c_str());

    {
        CAutoWriteLock nLocker(m_notingGuard);
        m_notifyIgnores.insert(crypt);
    }
    bool bRet = RunGPG(cmdlineBuf.get(), targetFolder);
    if (bRet)
    {
        if (resetArchAttr)
        {
            // Reset archive attribute on original file
            AdjustFileAttributes(orig.c_str(), FILE_ATTRIBUTE_ARCHIVE, 0);
        }

        // set the file timestamp
        int retry = 5;
        do
        {
            if (m_pProgDlg && m_pProgDlg->HasUserCancelled())
                break;
            CAutoFile hFileCrypt = CreateFile(crypt.c_str(), GENERIC_WRITE | GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
            if (hFileCrypt.IsValid())
            {
                bRet = !!SetFileTime(hFileCrypt, nullptr, nullptr, &fd.ft);
            }
            else
                bRet = false;
            if (!bRet)
                Sleep(200);
        } while (!bRet && (retry-- > 0));
        if (!bRet) // Should archive file be erased in this case (future sync will be unreliable due to incorrect date)?
            CCircularLog::Instance()(_T("INFO:    failed to set file time on %s"), crypt.c_str());
        CAutoWriteLock locker(m_failureGuard);
        m_failures.erase(orig);
    }
    else
    {
        // If encrypting failed, remove the leftover file
        DeleteFile(crypt.c_str());
        CAutoWriteLock locker(m_failureGuard);
        m_failures[orig] = Encrypt;
        CCircularLog::Instance()(L"ERROR:   Failed to encrypt file \"%s\" to \"%s\"", orig.c_str(), crypt.c_str());
    }
    return bRet;
}

bool CFolderSync::DecryptFile(const std::wstring& orig, const std::wstring& crypt, const std::wstring& password, const FileData& fd, bool useGpg)
{
    CTraceToOutputDebugString::Instance()(_T(__FUNCTION__) _T(": decrypt file %s to %s\n"), crypt.c_str(), orig.c_str());
    CCircularLog::Instance()(_T("INFO:    decrypt file %s to %s"), crypt.c_str(), orig.c_str());
    size_t slashPos = orig.find_last_of('\\');
    if (slashPos == std::string::npos)
        return false;
    std::wstring targetFolder = orig.substr(0, slashPos);
    size_t       bufLen       = orig.size() + crypt.size() + password.size() + 1000;
    auto         cmdlineBuf   = std::make_unique<wchar_t[]>(bufLen);

    if (!useGpg || password.empty())
    {
        if (password.empty())
            CCircularLog::Instance()(_T("WARNING: password is blank - NOT secure - force 7z not GPG"), crypt.c_str());

        auto progressFunc = [&](UInt64, UInt64, const std::wstring&) {
            if (m_pProgDlg && m_pProgDlg->HasUserCancelled())
                return E_ABORT;
            return S_OK;
        };

        C7Zip extractor;
        extractor.SetPassword(password);
        extractor.SetArchivePath(crypt);
        extractor.SetCompressionFormat(CompressionFormat::SevenZip, 9);
        extractor.SetCallback(progressFunc);
        CPathUtils::CreateRecursiveDirectory(targetFolder);
        if (extractor.Extract(targetFolder))
        {
            // Setting the file last write time is usually not required: 7zip will have set it based on archive content, even
            // for files with read-only attributes (code below logs error msg for those files).
            //
            // but it is possible that the encrypted file has the last-write-time changed (i.e. different from the source file).
            // so we check here if the file time is correct and if not, try to adjust it.
            int  retry = 5;
            bool bRet  = true;
            do
            {
                if (m_pProgDlg && m_pProgDlg->HasUserCancelled())
                    break;

                CAutoFile hFile = CreateFile(orig.c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
                if (hFile.IsValid())
                {
                    FILETIME ftCreate{};
                    bRet = !!GetFileTime(hFile, nullptr, nullptr, &ftCreate);
                    if (bRet)
                    {
                        bRet = CompareFileTime(&fd.ft, &ftCreate) == 0;
                    }
                    if (!bRet)
                    {
                        hFile.CloseHandle();
                        hFile = CreateFile(orig.c_str(), FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
                        bRet  = !!SetFileTime(hFile, nullptr, nullptr, &fd.ft);
                    }
                }
                else
                    bRet = false;
                if (!bRet)
                    Sleep(200);
            } while (!bRet && (retry-- > 0));
            if (!bRet)
                CCircularLog::Instance()(_T("ERROR:   failed to set file time on %s"), orig.c_str());
            CAutoWriteLock locker(m_failureGuard);
            m_failures.erase(orig);
            return true;
        }
        else
        {
            DeleteFile(orig.c_str());
            CAutoWriteLock locker(m_failureGuard);
            m_failures[orig] = Decrypt;
            CCircularLog::Instance()(L"ERROR:   Failed to decrypt file \"%s\" to \"%s\"", crypt.c_str(), orig.c_str());
            return false;
        }
    }

    swprintf_s(cmdlineBuf.get(), bufLen, L"\"%s\" --yes --batch --passphrase \"%s\" -o \"%s\" \"%s\" ", m_gnuPg.c_str(), password.c_str(), orig.c_str(), crypt.c_str());
    {
        CAutoWriteLock nLocker(m_notingGuard);
        m_notifyIgnores.insert(orig);
    }
    bool bRet = RunGPG(cmdlineBuf.get(), targetFolder);
    if (bRet)
    {
        // set the file timestamp
        int retry = 5;
        do
        {
            if (m_pProgDlg && m_pProgDlg->HasUserCancelled())
                break;
            CAutoFile hFile = CreateFile(orig.c_str(), GENERIC_WRITE | GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
            if (hFile.IsValid())
            {
                bRet = !!SetFileTime(hFile, nullptr, nullptr, &fd.ft);
            }
            else
                bRet = false;
            if (!bRet)
                Sleep(200);
        } while (!bRet && (retry-- > 0));
        if (!bRet)
            CCircularLog::Instance()(_T("ERROR:   failed to set file time on %s"), orig.c_str());
        CAutoWriteLock locker(m_failureGuard);
        m_failures.erase(orig);
    }
    else
    {
        DeleteFile(orig.c_str());
        CAutoWriteLock locker(m_failureGuard);
        m_failures[orig] = Decrypt;
        CCircularLog::Instance()(L"ERROR:   Failed to decrypt file \"%s\" to \"%s\"", crypt.c_str(), orig.c_str());
    }
    return bRet;
}

std::wstring CFolderSync::GetDecryptedFilename(const std::wstring& filename, const std::wstring& password, bool encryptName, bool newEncryption, bool use7Z, bool useGpg)
{
    std::wstring decryptName = filename;
    size_t       dotPos      = filename.find_last_of('.');
    std::wstring fName;
    if (dotPos != std::string::npos)
        fName = filename.substr(0, dotPos);
    else
        fName = filename;

    if (!encryptName)
    {
        std::wstring f = filename;
        std::transform(f.begin(), f.end(), f.begin(), ::towlower);
        if (useGpg)
        {
            size_t pos = f.rfind(L".gpg");
            if ((pos != std::string::npos) && (pos == (filename.size() - 4)))
            {
                return filename.substr(0, pos);
            }
        }
        else
        {
            if (use7Z)
            {
                size_t pos = f.rfind(L".7z");
                if ((pos != std::string::npos) && (pos == (filename.size() - 3)))
                {
                    return filename.substr(0, pos);
                }
            }
            else
            {
                size_t pos = f.rfind(L".cryptsync");
                if ((pos != std::string::npos) && (pos == (filename.size() - 10)))
                {
                    return filename.substr(0, pos);
                }
            }
        }

        return filename;
    }

    HCRYPTPROV hProv = NULL;
    // Get handle to user default provider.
    if (CryptAcquireContext(&hProv, nullptr, nullptr, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT | CRYPT_SILENT))
    {
        HCRYPTHASH hHash = NULL;
        // Create hash object.
        if (CryptCreateHash(hProv, CALG_MD5, 0, 0, &hHash))
        {
            // Hash password string.
            DWORD dwLength = static_cast<DWORD>(sizeof(WCHAR) * password.size());
            if (CryptHashData(hHash, reinterpret_cast<BYTE*>(const_cast<wchar_t*>(password.c_str())), dwLength, 0))
            {
                HCRYPTKEY hKey = NULL;
                // Create block cipher session key based on hash of the password.
                if (CryptDeriveKey(hProv, CALG_RC4, hHash, CRYPT_EXPORTABLE, &hKey))
                {
                    std::vector<std::wstring> names;
                    std::vector<std::wstring> decryptNames;
                    stringtok(names, fName, true, L"\\/");
                    for (auto it = names.cbegin(); it != names.cend(); ++it)
                    {
                        std::string name = CUnicodeUtils::StdGetUTF8(*it);
                        dwLength         = static_cast<DWORD>(name.size() + 1024); // 1024 bytes should be enough for padding
                        auto buffer      = std::make_unique<BYTE[]>(dwLength);

                        auto strIn       = std::make_unique<BYTE[]>(name.size() * sizeof(WCHAR) + 1);
                        if (newEncryption)
                        {
                            base4k::B4K_ENCODING_SETTINGS encodingSettings;
                            base4k::initialize(&encodingSettings, 2);
                            uint32_t ccData   = B4K_AUTO;
                            uint8_t* cDecoded = nullptr;
                            if (base4k::base4KDecode(reinterpret_cast<const uint16_t*>(it->data()), &ccData, &cDecoded) == base4k::B4K_SUCCESS)
                            {
                                memcpy(buffer.get(), cDecoded, ccData + 1LL);
                                CryptDecrypt(hKey, 0, true, 0, static_cast<BYTE*>(buffer.get()), &dwLength);
                                decryptName = CUnicodeUtils::StdGetUnicode(std::string(reinterpret_cast<char*>(buffer.get()), ccData));
                            }
                            else
                                decryptName = *it;
                            free(cDecoded);
                        }
                        else
                        {
                            if (CStringUtils::FromHexString(name, strIn.get()))
                            {
                                // copy encrypted password to temporary buffer
                                memcpy(buffer.get(), strIn.get(), name.size());
                                CryptDecrypt(hKey, 0, true, 0, static_cast<BYTE*>(buffer.get()), &dwLength);
                                decryptName = CUnicodeUtils::StdGetUnicode(std::string(reinterpret_cast<char*>(buffer.get()), name.size() / 2));
                            }
                        }
                        if (decryptName.empty() || (decryptName[0] != '*'))
                        {
                            if ((dotPos != std::string::npos) && ((it + 1) == names.cend()))
                            {
                                std::wstring s = *it;
                                s += filename.substr(dotPos);
                                decryptNames.push_back(s);
                            }
                            else
                                decryptNames.push_back(*it);
                        }
                        else
                            decryptNames.push_back(decryptName.substr(1)); // cut off the starting '*'
                    }
                    CryptDestroyKey(hKey); // Release provider handle.
                    decryptName.clear();
                    for (auto it = decryptNames.cbegin(); it != decryptNames.cend(); ++it)
                    {
                        if (!decryptName.empty())
                            decryptName += L"\\";
                        decryptName += *it;
                    }
                }
            }
            CryptDestroyHash(hHash); // Destroy session key.
        }
        CryptReleaseContext(hProv, 0);
    }
    else
        DebugBreak();

    if (decryptName.empty())
        decryptName = filename;

    return decryptName;
}

std::wstring CFolderSync::GetEncryptedFilename(const std::wstring& filename, const std::wstring& password, bool encryptName, bool newEncryption, bool use7Z, bool useGpg)
{
    std::wstring encryptFilename = filename;
    if (!encryptName)
    {
        std::wstring f = filename;
        std::transform(f.begin(), f.end(), f.begin(), ::towlower);

        if (useGpg)
        {
            encryptFilename += L".gpg";
            return encryptFilename;
        }
        else
        {
            if (use7Z)
            {
                encryptFilename += L".7z";
                return encryptFilename;
            }
            else
            {
                encryptFilename += L".cryptsync";
                return encryptFilename;
            }
        }
    }

    bool       bResult = true;

    HCRYPTPROV hProv   = NULL;
    // Get handle to user default provider.
    if (CryptAcquireContext(&hProv, nullptr, nullptr, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT | CRYPT_SILENT))
    {
        HCRYPTHASH hHash = NULL;
        // Create hash object.
        if (CryptCreateHash(hProv, CALG_MD5, 0, 0, &hHash))
        {
            // Hash password string.
            DWORD dwLength = static_cast<DWORD>(sizeof(WCHAR) * password.size());
            if (CryptHashData(hHash, reinterpret_cast<BYTE*>(const_cast<wchar_t*>(password.c_str())), dwLength, 0))
            {
                // Create block cipher session key based on hash of the password.
                HCRYPTKEY hKey = NULL;
                if (CryptDeriveKey(hProv, CALG_RC4, hHash, CRYPT_EXPORTABLE, &hKey))
                {
                    std::vector<std::wstring> names;
                    std::vector<std::wstring> encryptNames;
                    stringtok(names, filename, true, L"\\/");
                    for (auto it = names.cbegin(); it != names.cend(); ++it)
                    {
                        // Determine number of bytes to encrypt at a time.
                        std::string starName = "*";
                        starName += CUnicodeUtils::StdGetUTF8(*it);

                        dwLength    = static_cast<DWORD>(starName.size());
                        auto buffer = std::make_unique<BYTE[]>(dwLength + 1024LL);
                        memcpy(buffer.get(), starName.c_str(), dwLength);
                        // Encrypt data
                        if (CryptEncrypt(hKey, 0, true, 0, buffer.get(), &dwLength, dwLength + 1024))
                        {
                            if (newEncryption)
                            {
                                base4k::B4K_ENCODING_SETTINGS encodingSettings;
                                base4k::initialize(&encodingSettings, 2);
                                uint32_t  ccData   = dwLength;
                                uint16_t* cEncoded = nullptr;
                                if (base4k::base4kEncode(&encodingSettings, reinterpret_cast<const uint8_t*>(buffer.get()), &ccData, &cEncoded) == base4k::B4K_SUCCESS)
                                {
                                    OnOutOfScope(free(cEncoded));
                                    encryptFilename = reinterpret_cast<wchar_t*>(cEncoded);
                                    encryptNames.push_back(encryptFilename);
                                }
                            }
                            else
                            {
                                encryptFilename = CStringUtils::ToHexWString(buffer.get(), dwLength);
                                encryptNames.push_back(encryptFilename);
                            }
                        }
                        else
                        {
                            encryptNames.push_back(*it);
                            bResult = false;
                        }
                    }
                    encryptFilename.clear();
                    for (auto it = encryptNames.cbegin(); it != encryptNames.cend(); ++it)
                    {
                        if (!encryptFilename.empty())
                            encryptFilename += L"\\";
                        encryptFilename += *it;
                    }
                    if (useGpg)
                    {
                        encryptFilename += L".gpg";
                    }
                    else
                    {
                        if (use7Z)
                            encryptFilename += L".7z";
                        else
                            encryptFilename += L".cryptsync";
                    }
                    CryptDestroyKey(hKey); // Release provider handle.
                }
                else
                {
                    bResult = false;
                }
            }
            else
            {
                bResult = false;
            }
            CryptDestroyHash(hHash);
        }
        else
        {
            bResult = false;
        }
        CryptReleaseContext(hProv, 0);
    }
    else
        DebugBreak();
    if (bResult)
        return encryptFilename;
    return filename;
}

bool CFolderSync::RunGPG(LPWSTR cmdline, const std::wstring& cwd) const
{
    if (m_pProgDlg && m_pProgDlg->HasUserCancelled())
        return false;
    PROCESS_INFORMATION pi = {nullptr};

    CPathUtils::CreateRecursiveDirectory(cwd);
    if (CCreateProcessHelper::CreateProcess(m_gnuPg.c_str(), cmdline, NULL, &pi, true, BELOW_NORMAL_PRIORITY_CLASS | CREATE_UNICODE_ENVIRONMENT))
    {
        // wait until the process terminates
        DWORD waitRet = 0;
        do
        {
            waitRet = WaitForSingleObject(pi.hProcess, 2000);
            if (m_pProgDlg && m_pProgDlg->HasUserCancelled())
            {
                TerminateProcess(pi.hProcess, 1);
                break;
            }
        } while (waitRet == WAIT_TIMEOUT);

        DWORD exitCode = 0;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);

        return (exitCode == 0);
    }

    return false;
}

void CFolderSync::AdjustFileAttributes(const std::wstring& fName, DWORD dwFileAttributesToClear, DWORD dwFileAttributesToSet) const
{
    // Adjust file attributes on file without impacting file times
    WIN32_FILE_ATTRIBUTE_DATA fData = {};
    DWORD                     error = 0;

    int                       retry = 5;
    bool                      bRet  = true;
    do
    {
        if (m_pProgDlg && m_pProgDlg->HasUserCancelled())
            break;
        if ((bRet = GetFileAttributesEx(fName.c_str(), GetFileExInfoStandard, &fData)) != 0)
        {
            if (((fData.dwFileAttributes & dwFileAttributesToSet) == dwFileAttributesToSet) && ((fData.dwFileAttributes & dwFileAttributesToClear) == 0))
            {
                // Attribute already set / cleared as requested
                // Commented out (noisy log enty) CTraceToOutputDebugString::Instance()(_T(__FUNCTION__) _T(": Attribute %d already set correctly on file %s \n"), dwFileAttributesToSet, fName.c_str());
                return;
            }

            if ((dwFileAttributesToClear & dwFileAttributesToSet) != 0)
            {
                CTraceToOutputDebugString::Instance()(_T(__FUNCTION__) _T(": Unexpected usage: clearing and setting same attribute on %s, dwFileAttributesToClear=%d, dwFileAttributesToSet (will be set)=%d \n"), fName.c_str(), dwFileAttributesToClear, dwFileAttributesToSet);
            }

            fData.dwFileAttributes &= (~dwFileAttributesToClear);
            bRet = SetFileAttributes(fName.c_str(), fData.dwFileAttributes | dwFileAttributesToSet);
        }
        error = ::GetLastError();
        if (!bRet)
            Sleep(200);
    } while (!bRet && (retry-- > 0));

    if (!bRet)
    {
        _com_error comError(error);
        LPCTSTR    comErrorText = comError.ErrorMessage();

        CCircularLog::Instance()(_T("INFO:    failed to adjust attributes on %s (%s)"), fName.c_str(), comErrorText);
        CTraceToOutputDebugString::Instance()(_T(__FUNCTION__) _T(": Unable to adjust file attributes on %s, dwFileAttributesToClear=%d, dwFileAttributesToSet=%d \n"), fName.c_str(), dwFileAttributesToClear, dwFileAttributesToSet);
    }
    else
    {
        bRet            = false;
        // Use FILE_WRITE_ATTRIBUTES below to prevent sharing violation if working on
        // file open by another application
        CAutoFile hFile = CreateFile(fName.c_str(), FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
        error           = ::GetLastError();
        if (hFile.IsValid())
        {
            retry = 5;
            do
            {
                if (m_pProgDlg && m_pProgDlg->HasUserCancelled())
                    break;
                bRet  = !!SetFileTime(hFile, &fData.ftCreationTime, &fData.ftLastAccessTime, &fData.ftLastWriteTime);
                error = ::GetLastError();
                if (!bRet)
                    Sleep(200);
            } while (!bRet && (retry-- > 0));
            hFile.CloseHandle();
        }
        if (!bRet)
        {
            _com_error comError(error);
            LPCTSTR    comErrorText = comError.ErrorMessage();
            CCircularLog::Instance()(_T("INFO:    failed to set file time on %s while adjusting its attributes (%s)"), fName.c_str(), comErrorText);
            CTraceToOutputDebugString::Instance()(_T(__FUNCTION__) _T(": Unable to set file time on %s (%s)\n"), fName.c_str(), comErrorText);
        }
        else
            CCircularLog::Instance()(_T("INFO:    successfully adjusted attribute on %s"), fName.c_str());
    }
}

bool CFolderSync::DeletePathToTrash(const std::wstring& path)
{
    if (path.starts_with(L"\\\\?\\UNC"))
    {
        std::wstring newPath = L"\\" + path.substr(7);
        return DeletePathToTrash(newPath);
    }
    if (path.starts_with(L"\\\\?\\"))
    {
        std::wstring newPath = path.substr(4);
        return DeletePathToTrash(newPath);
    }
    IFileOperationPtr pfo = nullptr;
    auto              hr  = pfo.CreateInstance(CLSID_FileOperation, nullptr, CLSCTX_ALL);
    if (SUCCEEDED(hr))
    {
        DWORD flags = FOF_ALLOWUNDO | FOF_FILESONLY | FOF_NOCONFIRMATION | FOF_NO_CONNECTED_ELEMENTS | FOF_NOERRORUI | FOF_SILENT | FOF_NORECURSION | FOFX_RECYCLEONDELETE;
        pfo->SetOperationFlags(flags);
        IShellItemPtr psiFrom = nullptr;
        hr                    = SHCreateItemFromParsingName(path.c_str(), nullptr, IID_PPV_ARGS(&psiFrom));
        if ((hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)) || (hr == HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND)))
            return true;

        if (SUCCEEDED(hr))
            hr = pfo->DeleteItem(psiFrom, nullptr);

        if (SUCCEEDED(hr))
        {
            hr = pfo->PerformOperations();
            if (SUCCEEDED(hr))
            {
                BOOL fAnyOperationsAborted = false;
                pfo->GetAnyOperationsAborted(&fAnyOperationsAborted);
                if (!fAnyOperationsAborted)
                    return true;
            }
        }
    }
    // try the SHFileOperation
    FILEOP_FLAGS   flags = FOF_ALLOWUNDO | FOF_FILESONLY | FOF_NOCONFIRMATION | FOF_NO_CONNECTED_ELEMENTS | FOF_NOERRORUI | FOF_SILENT | FOF_NORECURSION;
    SHFILEOPSTRUCT fop   = {nullptr};
    fop.wFunc            = FO_DELETE;
    fop.fFlags           = flags;
    auto delBuf          = std::make_unique<wchar_t[]>(path.size() + 2);
    wcscpy_s(delBuf.get(), path.size() + 2, path.c_str());
    delBuf[path.size()]     = 0;
    delBuf[path.size() + 1] = 0;
    fop.pFrom               = delBuf.get();
    return ((SHFileOperation(&fop) == 0) && (fop.fAnyOperationsAborted == FALSE));
}

std::map<std::wstring, SyncOp> CFolderSync::GetFailures()
{
    CAutoReadLock locker(m_failureGuard);
    return m_failures;
}

size_t CFolderSync::GetFailureCount()
{
    CAutoReadLock locker(m_failureGuard);
    return m_failures.size();
}

std::set<std::wstring> CFolderSync::GetNotifyIgnores()
{
    CAutoWriteLock locker(m_notingGuard);
    auto           ignCopy = m_notifyIgnores;
    m_notifyIgnores.clear();
    return ignCopy;
}

std::wstring CFolderSync::GetFileTimeStringForLog(const FILETIME& ft)
{
    SYSTEMTIME stUtc, stLocal = {0, 0, 0, 0, 0, 0, 0, 0};
    FileTimeToSystemTime(&ft, &stUtc);
    SystemTimeToTzSpecificLocalTime(nullptr, &stUtc, &stLocal);

    return CStringUtils::Format(L"%02d.%02d.%02d - %02d:%02d:%02d:%03d",
                                stLocal.wDay, stLocal.wMonth, stLocal.wYear,
                                stLocal.wHour, stLocal.wMinute, stLocal.wSecond, stLocal.wMilliseconds);
}
