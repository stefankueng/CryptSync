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

#include <process.h>
#include <shlobj.h>
#include <cctype>
#include <algorithm>


CFolderSync::CFolderSync(void)
    : m_sevenzip(L"%ProgramFiles%\\7-zip\\7z.exe")
    , m_GnuPG(L"%ProgramFiles%\\GNU\\GnuPG\\Pub\\gpg.exe")
    , m_parentWnd(NULL)
    , m_TrayWnd(NULL)
    , m_pProgDlg(NULL)
    , m_progress(0)
    , m_progressTotal(1)
    , m_bRunning(FALSE)
{
    wchar_t buf[1024] = {0};
    GetModuleFileName(NULL, buf, 1024);
    std::wstring dir = buf;
    dir = dir.substr(0, dir.find_last_of('\\'));
    m_sevenzip = dir + L"\\7z.exe";
    if (!PathFileExists(m_sevenzip.c_str()))
    {
        m_sevenzip = CStringUtils::ExpandEnvironmentStrings(L"%ProgramFiles%\\7-zip\\7z.exe");
        if (!PathFileExists(m_sevenzip.c_str()))
        {
#ifdef _WIN64
            m_sevenzip = L"%ProgramFiles(x86)%\\7-zip\\7z.exe";
#else
            m_sevenzip = L"%ProgramW6432%\\7-zip\\7z.exe";
#endif
            m_sevenzip = CStringUtils::ExpandEnvironmentStrings(m_sevenzip);
        }
    }
    m_GnuPG = CStringUtils::ExpandEnvironmentStrings(m_GnuPG);
    if (!PathFileExists(m_GnuPG.c_str()))
    {
#ifdef _WIN64
        m_GnuPG = L"%ProgramFiles(x86)%\\GNU\\GnuPG\\Pub\\gpg.exe";
#else
        m_GnuPG = L"%ProgramW6432%\\GNU\\GnuPG\\Pub\\gpg.exe";
#endif
        m_GnuPG = CStringUtils::ExpandEnvironmentStrings(m_GnuPG);
        if (!PathFileExists(m_GnuPG.c_str()))
        {
            // try the old version 1 of gpg
            m_GnuPG = L"%ProgramFiles%\\GNU\\GnuPG\\gpg.exe";
            m_GnuPG = CStringUtils::ExpandEnvironmentStrings(m_GnuPG);
            if (!PathFileExists(m_GnuPG.c_str()))
            {
#ifdef _WIN64
                m_GnuPG = L"%ProgramW6432%\\GNU\\GnuPG\\gpg.exe";
#else
                m_GnuPG = L"%ProgramW6432%\\GNU\\GnuPG\\gpg.exe";
#endif
                m_GnuPG = CStringUtils::ExpandEnvironmentStrings(m_GnuPG);
                if (!PathFileExists(m_GnuPG.c_str()))
                    m_GnuPG = dir + L"\\gpg.exe";
            }
        }
    }
}


CFolderSync::~CFolderSync(void)
{
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

void CFolderSync::SyncFolders( const PairVector& pv, HWND hWnd )
{
    if (m_bRunning)
    {
        Stop();
    }
    CAutoWriteLock locker(m_guard);
    m_pairs = pv;
    m_parentWnd = hWnd;
    unsigned int threadId = 0;
    InterlockedExchange(&m_bRunning, TRUE);
    m_hThread = (HANDLE)_beginthreadex(NULL, 0, SyncFolderThreadEntry, this, 0, &threadId);
}

void CFolderSync::SyncFoldersWait( const PairVector& pv, HWND hWnd )
{
    CAutoWriteLock locker(m_guard);
    m_pairs = pv;
    m_parentWnd = hWnd;
    InterlockedExchange(&m_bRunning, TRUE);
    SyncFolderThread();
}

unsigned int CFolderSync::SyncFolderThreadEntry(void* pContext)
{
    ((CFolderSync*)pContext)->SyncFolderThread();
    _endthreadex(0);
    return 0;
}

void CFolderSync::SyncFolderThread()
{
    PairVector pv;
    {
        CAutoReadLock locker(m_guard);
        pv = m_pairs;
    }
    if (m_parentWnd)
    {
        CoInitializeEx(0, COINIT_APARTMENTTHREADED);
        m_progress = 0;
        m_progressTotal = 1;
        m_pProgDlg = new CProgressDlg();
        m_pProgDlg->SetTitle(L"Syncing folders");
        m_pProgDlg->SetLine(0, L"scanning...");
        m_pProgDlg->SetProgress(m_progress, m_progressTotal);
        m_pProgDlg->ShowModal(m_parentWnd);
    }
    {
        CAutoWriteLock locker(m_failureguard);
        m_failures.clear();
    }
    for (auto it = pv.cbegin(); (it != pv.cend()) && m_bRunning; ++it)
    {
        {
            CAutoWriteLock locker(m_guard);
            m_currentPath = *it;
        }
        SyncFolder(*it);
        {
            CAutoWriteLock locker(m_guard);
            m_currentPath = PairData();
        }
    }
    if (m_pProgDlg)
    {
        m_pProgDlg->Stop();
        delete m_pProgDlg;
        m_pProgDlg = NULL;
        CoUninitialize();
    }
    PostMessage(m_parentWnd, WM_THREADENDED, 0, 0);
    m_parentWnd = NULL;
    InterlockedExchange(&m_bRunning, FALSE);
}


void CFolderSync::SyncFile( const std::wstring& path )
{
    // check if the path notification comes from a folder that's
    // currently synced in the sync thread
    {
        std::wstring s = m_currentPath.origpath;
        if (!s.empty())
        {
            if ((path.size() > s.size()) &&
                (_wcsicmp(s.c_str(), path.substr(0, s.size()).c_str())==0))
                return;
        }
        s = m_currentPath.cryptpath;
        if (!s.empty())
        {
            if ((path.size() > s.size()) &&
                (_wcsicmp(s.c_str(), path.substr(0, s.size()).c_str())==0))
                return;
        }
    }

    PairData pt;
    CAutoReadLock locker(m_guard);
    for (auto it = m_pairs.cbegin(); it != m_pairs.cend(); ++it)
    {
        std::wstring s = it->origpath;
        if (path.size() > s.size())
        {
            if ((_wcsicmp(path.substr(0, s.size()).c_str(), s.c_str())==0)&&(path[s.size()] == '\\'))
            {
                pt = *it;
                SyncFile(path, pt);
                continue;
            }
        }
        s = it->cryptpath;
        if (path.size() > s.size())
        {
            if ((_wcsicmp(path.substr(0, s.size()).c_str(), s.c_str())==0)&&(path[s.size()] == '\\'))
            {
                pt = *it;
                SyncFile(path, pt);
                continue;
            }
        }
    }
}

void CFolderSync::SyncFile( const std::wstring& path, const PairData& pt )
{
    std::wstring orig  = pt.origpath;
    std::wstring crypt = pt.cryptpath;
    if (orig.empty() || crypt.empty())
        return;
    if (pt.IsIgnored(path))
        return;
    bool bCopyOnly = pt.IsCopyOnly(path);
    if ((orig.size() < path.size())&&(_wcsicmp(path.substr(0, orig.size()).c_str(), orig.c_str())==0))
    {
        if (bCopyOnly)
            crypt = CPathUtils::Append(crypt, path.substr(orig.size()));
        else
            crypt = CPathUtils::Append(crypt, GetEncryptedFilename(path.substr(orig.size()), pt.password, pt.encnames, pt.use7z, pt.useGPG));
        orig = path;
    }
    else
    {
        if (bCopyOnly)
            orig = CPathUtils::Append(orig, path.substr(crypt.size()));
        else
            orig = CPathUtils::Append(orig, GetDecryptedFilename(path.substr(crypt.size()), pt.password, pt.encnames, pt.use7z, pt.useGPG));
        crypt = path;
    }
    crypt = CPathUtils::AdjustForMaxPath(crypt);
    orig = CPathUtils::AdjustForMaxPath(orig);

    WIN32_FILE_ATTRIBUTE_DATA fdataorig  = {0};
    WIN32_FILE_ATTRIBUTE_DATA fdatacrypt = {0};
    bool bOrigMissing = false;
    bool bCryptMissing = false;
    if (!GetFileAttributesEx(orig.c_str(),  GetFileExInfoStandard, &fdataorig))
    {
        DWORD lastError = GetLastError();
        if (lastError == ERROR_ACCESS_DENIED)
            return;
        bOrigMissing = (lastError == ERROR_FILE_NOT_FOUND);
    }
    if (!GetFileAttributesEx(crypt.c_str(),  GetFileExInfoStandard, &fdatacrypt))
    {
        DWORD lastError = GetLastError();
        if (lastError == ERROR_ACCESS_DENIED)
            return;
        bCryptMissing = (lastError == ERROR_FILE_NOT_FOUND);
    }

    if ((fdataorig.ftLastWriteTime.dwLowDateTime == 0)&&(fdataorig.ftLastWriteTime.dwHighDateTime == 0)&&
        (_wcsicmp(orig.c_str(), path.c_str())==0) && bOrigMissing)
    {
        // original file got deleted
        // delete the encrypted file
        {
            CAutoWriteLock nlocker(m_notignguard);
            m_notifyignores.insert(crypt);
        }
        CTraceToOutputDebugString::Instance()(_T(__FUNCTION__) _T(": file %s does not exist, delete file %s\n"), orig.c_str(), crypt.c_str());
        CCircularLog::Instance()(_T("file %s does not exist, delete file %s"), orig.c_str(), crypt.c_str());

        SHFILEOPSTRUCT fop = {0};
        fop.wFunc = FO_DELETE;
        fop.fFlags = FOF_ALLOWUNDO | FOF_FILESONLY | FOF_NOCONFIRMATION | FOF_NO_CONNECTED_ELEMENTS | FOF_NOERRORUI | FOF_SILENT | FOF_NORECURSION;
        std::unique_ptr<wchar_t[]> delbuf(new wchar_t[crypt.size()+2]);
        wcscpy_s(delbuf.get(), crypt.size()+2, crypt.c_str());
        delbuf[crypt.size()] = 0;
        delbuf[crypt.size()+1] = 0;
        fop.pFrom = delbuf.get();
        if (SHFileOperation(&fop))
        {
            // in case the notification was for a folder that got removed,
            // the GetDecryptedFilename() call above added the .cryptsync extension which
            // folders don't have. Remove that extension and try deleting again.
            crypt = crypt.substr(0, crypt.find_last_of('.'));
            wcscpy_s(delbuf.get(), crypt.size()+2, crypt.c_str());
            delbuf[crypt.size()] = 0;
            delbuf[crypt.size()+1] = 0;
            fop.pFrom = delbuf.get();
            if (SHFileOperation(&fop))
            {
                // could not delete file to the trashbin, so delete it directly
                DeleteFile(crypt.c_str());
                DeleteFile(delbuf.get());
            }
        }
        return;
    }
    else if ((fdatacrypt.ftLastWriteTime.dwLowDateTime == 0)&&(fdatacrypt.ftLastWriteTime.dwHighDateTime == 0)&&
        (_wcsicmp(crypt.c_str(), path.c_str())==0) && bCryptMissing)
    {
        // encrypted file got deleted
        // delete the original file as well
        {
            CAutoWriteLock nlocker(m_notignguard);
            m_notifyignores.insert(orig);
        }
        // check if there's an unencrypted file instead of an encrypted one in the encrypted folder
        if (!bCopyOnly)
        {
            std::wstring cryptnot = crypt.substr(0, crypt.find_last_of('.'));
            if (!GetFileAttributesEx(cryptnot.c_str(),  GetFileExInfoStandard, &fdatacrypt))
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
            CCircularLog::Instance()(_T("file %s does not exist, delete file %s"), crypt.c_str(), orig.c_str());

            SHFILEOPSTRUCT fop = {0};
            fop.wFunc = FO_DELETE;
            fop.fFlags = FOF_ALLOWUNDO | FOF_FILESONLY | FOF_NOCONFIRMATION | FOF_NO_CONNECTED_ELEMENTS | FOF_NOERRORUI | FOF_SILENT | FOF_NORECURSION;
            std::unique_ptr<wchar_t[]> delbuf(new wchar_t[orig.size()+2]);
            wcscpy_s(delbuf.get(), orig.size()+2, orig.c_str());
            delbuf[orig.size()] = 0;
            delbuf[orig.size()+1] = 0;
            fop.pFrom = delbuf.get();
            if (SHFileOperation(&fop))
            {
                // could not delete file to the trashbin, so delete it directly
                DeleteFile(orig.c_str());
            }
            return;
        }
    }

    if (fdataorig.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        return;
    if (fdatacrypt.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        return;
    LONG cmp = CompareFileTime(&fdataorig.ftLastWriteTime, &fdatacrypt.ftLastWriteTime);
    if (pt.FAT)
    {
        // round up to two seconds accuracy
        ULONGLONG qwResult;
        qwResult = (((ULONGLONG) fdataorig.ftLastWriteTime.dwHighDateTime) << 32) + fdataorig.ftLastWriteTime.dwLowDateTime;
        if (qwResult % 20000000UL)
        {
            qwResult += 20000000UL;
            qwResult /= 20000000UL;
            qwResult *= 20000000UL;
        }
        fdataorig.ftLastWriteTime.dwLowDateTime  = (DWORD) (qwResult & 0xFFFFFFFF );
        fdataorig.ftLastWriteTime.dwHighDateTime = (DWORD) (qwResult >> 32 );

        ULONGLONG qwResult2 = (((ULONGLONG)fdatacrypt.ftLastWriteTime.dwHighDateTime) << 32) + fdatacrypt.ftLastWriteTime.dwLowDateTime;
        if (qwResult2 % 20000000UL)
        {
            qwResult2 += 20000000UL;
            qwResult2 /= 20000000UL;
            qwResult2 *= 20000000UL;
        }
        fdatacrypt.ftLastWriteTime.dwLowDateTime = (DWORD)(qwResult2 & 0xFFFFFFFF);
        fdatacrypt.ftLastWriteTime.dwHighDateTime = (DWORD)(qwResult2 >> 32);
        cmp = CompareFileTime(&fdataorig.ftLastWriteTime, &fdatacrypt.ftLastWriteTime);

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
    if ((cmp < 0) && (!pt.oneway))
    {
        CCircularLog::Instance()(L"original file is older: %s : %s, %s : %s",
                                 crypt.c_str(), GetFileTimeStringForLog(fdatacrypt.ftLastWriteTime).c_str(),
                                 orig.c_str(), GetFileTimeStringForLog(fdataorig.ftLastWriteTime).c_str());
        // original file is older than the encrypted file
        // decrypt the file
        FileData fd;
        fd.ft = fdatacrypt.ftLastWriteTime;
        if (bCopyOnly)
        {
            CCircularLog::Instance()(_T("copy file %s to %s"), crypt.c_str(), orig.c_str());
            if (!CopyFile(crypt.c_str(), orig.c_str(), FALSE))
            {
                std::wstring targetfolder = orig.substr(0, orig.find_last_of('\\'));
                SHCreateDirectory(NULL, targetfolder.c_str());
                CopyFile(crypt.c_str(), orig.c_str(), FALSE);
            }
        }
        else
            DecryptFile(orig, crypt, pt.password, fd, pt.useGPG);
    }
    else if ((cmp > 0) || ((cmp < 0) && pt.oneway))
    {
        CCircularLog::Instance()(L"encrypted file is older: %s : %s, %s : %s",
                                 orig.c_str(), GetFileTimeStringForLog(fdataorig.ftLastWriteTime).c_str(),
                                 crypt.c_str(), GetFileTimeStringForLog(fdatacrypt.ftLastWriteTime).c_str());
        // encrypted file is older than the original file
        // encrypt the file
        FileData fd;
        fd.ft = fdataorig.ftLastWriteTime;
        if (bCopyOnly)
        {
            CCircularLog::Instance()(_T("copy file %s to %s"), orig.c_str(), crypt.c_str());
            if (!CopyFile(orig.c_str(), crypt.c_str(), FALSE))
            {
                std::wstring targetfolder = crypt.substr(0, crypt.find_last_of('\\'));
                SHCreateDirectory(NULL, targetfolder.c_str());
                CopyFile(orig.c_str(), crypt.c_str(), FALSE);
            }
        }
        else
            EncryptFile(orig, crypt, pt.password, fd, pt.useGPG);
    }
    else if (cmp == 0)
    {
        // files are identical (have the same last-write-time):
        // nothing to do.
    }
}

void CFolderSync::SyncFolder( const PairData& pt )
{
    if (m_pProgDlg)
    {
        m_pProgDlg->SetLine(0, L"scanning...");
        m_pProgDlg->SetLine(2, L"");
        m_pProgDlg->SetProgress(m_progress, m_progressTotal);
    }
    if (m_TrayWnd)
        PostMessage(m_TrayWnd, WM_PROGRESS, m_progress, m_progressTotal);
    if (GetFileAttributes(CPathUtils::AdjustForMaxPath(pt.origpath).c_str()) == INVALID_FILE_ATTRIBUTES)
    {
        CCircularLog::Instance()(L"error accessing path \"%s\", skipped", pt.origpath.c_str());
        return;
    }
    DWORD dwErr = 0;
    auto origFileList  = GetFileList(true, pt.origpath, pt.password, pt.encnames, pt.use7z, pt.useGPG, dwErr);
    if (dwErr)
    {
        CCircularLog::Instance()(L"error enumerating path \"%s\", skipped", pt.origpath.c_str());
        return;
    }
    auto cryptFileList = GetFileList(false, pt.cryptpath, pt.password, pt.encnames, pt.use7z, pt.useGPG, dwErr);
    if (dwErr)
    {
        CCircularLog::Instance()(L"error enumerating path \"%s\", skipped", pt.cryptpath.c_str());
        return;
    }

    m_progressTotal += DWORD(origFileList.size() + cryptFileList.size());

    for (auto it = origFileList.cbegin(); (it != origFileList.cend()) && m_bRunning; ++it)
    {
        if (m_TrayWnd)
            PostMessage(m_TrayWnd, WM_PROGRESS, m_progress, m_progressTotal);
        if (m_pProgDlg)
        {
            m_pProgDlg->SetLine(0, L"syncing files");
            m_pProgDlg->SetLine(2, it->first.c_str(), true);
            m_pProgDlg->SetProgress(m_progress, m_progressTotal);
            if (m_pProgDlg->HasUserCancelled())
            {
                if (m_TrayWnd)
                    PostMessage(m_TrayWnd, WM_PROGRESS, 0, 0);
                break;
            }
        }
        ++m_progress;

        if (CIgnores::Instance().IsIgnored(CPathUtils::Append(pt.origpath, it->first)))
            continue;
        if (pt.IsIgnored(CPathUtils::Append(pt.origpath, it->first)))
            continue;
        bool bCopyOnly = pt.IsCopyOnly(CPathUtils::Append(pt.origpath, it->first));
        auto cryptit = cryptFileList.find(it->first);
        if (cryptit == cryptFileList.end())
        {
            // file does not exist in the encrypted folder:
            // encrypt the file
            CTraceToOutputDebugString::Instance()(_T(__FUNCTION__) _T(": file %s does not exist in encrypted folder\n"), it->first.c_str());
            if (bCopyOnly)
            {
                std::wstring cryptpath = CPathUtils::AdjustForMaxPath(CPathUtils::Append(pt.cryptpath, it->first));
                std::wstring origpath = CPathUtils::AdjustForMaxPath(CPathUtils::Append(pt.origpath, it->first));
                CCircularLog::Instance()(_T("copy file %s to %s"), origpath.c_str(), cryptpath.c_str());
                if (!CopyFile(origpath.c_str(), cryptpath.c_str(), FALSE))
                {
                    std::wstring targetfolder = cryptpath;
                    targetfolder = targetfolder.substr(0, targetfolder.find_last_of('\\'));
                    SHCreateDirectory(NULL, targetfolder.c_str());
                    CopyFile(origpath.c_str(), cryptpath.c_str(), FALSE);
                }
            }
            else
            {
                std::wstring cryptpath = CPathUtils::AdjustForMaxPath(CPathUtils::Append(pt.cryptpath, GetEncryptedFilename(it->first, pt.password, pt.encnames, pt.use7z, pt.useGPG)));
                std::wstring origpath = CPathUtils::AdjustForMaxPath(CPathUtils::Append(pt.origpath, it->first));
                EncryptFile(origpath, cryptpath, pt.password, it->second, pt.useGPG);
            }
        }
        else
        {
            LONG cmp = 0;
            if (pt.FAT)
            {
                // round up to two seconds accuracy
                FILETIME ft1;
                ft1.dwLowDateTime = it->second.ft.dwLowDateTime;
                ft1.dwHighDateTime = it->second.ft.dwHighDateTime;
                FILETIME ft2;
                ft2.dwLowDateTime = cryptit->second.ft.dwLowDateTime;
                ft2.dwHighDateTime = cryptit->second.ft.dwHighDateTime;

                ULONGLONG qwResult;
                qwResult = (((ULONGLONG) ft1.dwHighDateTime) << 32) + ft1.dwLowDateTime;
                if (qwResult % 20000000UL)
                {
                    qwResult += 20000000UL;
                    qwResult /= 20000000UL;
                    qwResult *= 20000000UL;
                }
                ft1.dwLowDateTime  = (DWORD) (qwResult & 0xFFFFFFFF );
                ft1.dwHighDateTime = (DWORD) (qwResult >> 32 );

                ULONGLONG qwResult2 = (((ULONGLONG)ft2.dwHighDateTime) << 32) + ft2.dwLowDateTime;
                if (qwResult2 % 20000000UL)
                {
                    qwResult2 += 20000000UL;
                    qwResult2 /= 20000000UL;
                    qwResult2 *= 20000000UL;
                }
                ft2.dwLowDateTime  = (DWORD) (qwResult2 & 0xFFFFFFFF );
                ft2.dwHighDateTime = (DWORD) (qwResult2 >> 32 );

                cmp = CompareFileTime(&ft1, &ft2);
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
                cmp = CompareFileTime(&it->second.ft, &cryptit->second.ft);
            if (cmp < 0)
            {
                CCircularLog::Instance()(L"original file is older: %s : %s, %s : %s",
                                         (pt.cryptpath + L"\\" + it->first).c_str(), GetFileTimeStringForLog(cryptit->second.ft).c_str(),
                                         (pt.origpath + L"\\" + it->first).c_str(), GetFileTimeStringForLog(it->second.ft).c_str());
                // original file is older than the encrypted file
                // decrypt the file
                CTraceToOutputDebugString::Instance()(_T(__FUNCTION__) _T(": file %s is older than its encrypted partner\n"), it->first.c_str());
                if (bCopyOnly)
                {
                    std::wstring cryptpath = CPathUtils::AdjustForMaxPath(CPathUtils::Append(pt.cryptpath, it->first));
                    std::wstring origpath = CPathUtils::AdjustForMaxPath(CPathUtils::Append(pt.origpath, it->first));
                    CCircularLog::Instance()(_T("copy file %s to %s"), cryptpath.c_str(), origpath.c_str());
                    if (!CopyFile(cryptpath.c_str(), origpath.c_str(), FALSE))
                    {
                        std::wstring targetfolder = pt.origpath + L"\\" + it->first;
                        targetfolder = targetfolder.substr(0, targetfolder.find_last_of('\\'));
                        SHCreateDirectory(NULL, targetfolder.c_str());
                        CopyFile(cryptpath.c_str(), origpath.c_str(), FALSE);
                    }
                }
                else
                {
                    std::wstring cryptpath = CPathUtils::AdjustForMaxPath(CPathUtils::Append(pt.cryptpath, GetEncryptedFilename(it->first, pt.password, pt.encnames, pt.use7z, pt.useGPG)));
                    std::wstring origpath = CPathUtils::AdjustForMaxPath(CPathUtils::Append(pt.origpath, it->first));
                    DecryptFile(origpath, cryptpath, pt.password, it->second, pt.useGPG);
                }
            }
            else if (cmp > 0)
            {
                CCircularLog::Instance()(L"encrypted file is older: %s : %s, %s : %s",
                                         (pt.origpath + L"\\" + it->first).c_str(), GetFileTimeStringForLog(it->second.ft).c_str(),
                                         (pt.cryptpath + L"\\" + it->first).c_str(), GetFileTimeStringForLog(cryptit->second.ft).c_str());
                // encrypted file is older than the original file
                // encrypt the file
                CTraceToOutputDebugString::Instance()(_T(__FUNCTION__) _T(": file %s is newer than its encrypted partner\n"), it->first.c_str());
                if (bCopyOnly)
                {
                    std::wstring cryptpath = CPathUtils::AdjustForMaxPath(CPathUtils::Append(pt.cryptpath, it->first));
                    std::wstring origpath = CPathUtils::AdjustForMaxPath(CPathUtils::Append(pt.origpath, it->first));
                    CCircularLog::Instance()(_T("copy file %s to %s"), origpath.c_str(), cryptpath.c_str());
                    if (!CopyFile(origpath.c_str(), cryptpath.c_str(), FALSE))
                    {
                        std::wstring targetfolder = pt.cryptpath + L"\\" + it->first;
                        targetfolder = targetfolder.substr(0, targetfolder.find_last_of('\\'));
                        SHCreateDirectory(NULL, targetfolder.c_str());
                        CopyFile(origpath.c_str(), cryptpath.c_str(), FALSE);
                    }
                }
                else
                {
                    std::wstring cryptpath = CPathUtils::Append(pt.cryptpath, GetEncryptedFilename(it->first, pt.password, pt.encnames, pt.use7z, pt.useGPG));
                    std::wstring origpath = CPathUtils::Append(pt.origpath, it->first);
                    EncryptFile(origpath, cryptpath, pt.password, it->second, pt.useGPG);
                }
            }
            else if (cmp == 0)
            {
                // files are identical (have the same last-write-time):
                // nothing to do.
            }
        }
    }
    // now go through the encrypted file list and if there's a file that's not in the original file list,
    // decrypt it
    for (auto it = cryptFileList.cbegin(); (it != cryptFileList.cend()) && m_bRunning; ++it)
    {
        if (m_TrayWnd)
            PostMessage(m_TrayWnd, WM_PROGRESS, m_progress, m_progressTotal);
        if (m_pProgDlg)
        {
            m_pProgDlg->SetLine(0, L"syncing files");
            m_pProgDlg->SetLine(2, it->first.c_str(), true);
            m_pProgDlg->SetProgress(m_progress, m_progressTotal);
            if (m_pProgDlg->HasUserCancelled())
            {
                if (m_TrayWnd)
                    PostMessage(m_TrayWnd, WM_PROGRESS, 0, 0);
                break;
            }
        }
        ++m_progress;

        if (CIgnores::Instance().IsIgnored(CPathUtils::Append(pt.origpath, it->first)))
            continue;
        if (pt.IsIgnored(CPathUtils::Append(pt.origpath, it->first)))
            continue;
        bool bCopyOnly = pt.IsCopyOnly(CPathUtils::Append(pt.origpath, it->first));
        auto origit = origFileList.find(it->first);
        if (origit == origFileList.end())
        {
            // file does not exist in the original folder:
            if (pt.oneway && !origFileList.empty())
            {
                // remove the encrypted file
                CTraceToOutputDebugString::Instance()(_T(__FUNCTION__) _T(": counterpart of file %s does not exist in src folder, delete file\n"), it->first.c_str());
                CCircularLog::Instance()(_T("counterpart of file %s does not exist in src folder, delete file"), it->first.c_str());
                SHFILEOPSTRUCT fop = {0};
                fop.wFunc = FO_DELETE;
                fop.fFlags = FOF_ALLOWUNDO | FOF_FILESONLY | FOF_NOCONFIRMATION | FOF_NO_CONNECTED_ELEMENTS | FOF_NOERRORUI | FOF_SILENT | FOF_NORECURSION;
                std::wstring crypt = CPathUtils::Append(pt.cryptpath, it->second.filerelpath);
                std::unique_ptr<wchar_t[]> delbuf(new wchar_t[crypt.size()+2]);
                wcscpy_s(delbuf.get(), crypt.size()+2, crypt.c_str());
                delbuf[crypt.size()] = 0;
                delbuf[crypt.size()+1] = 0;
                fop.pFrom = delbuf.get();
                {
                    CAutoWriteLock nlocker(m_notignguard);
                    m_notifyignores.insert(crypt);
                }
                if (SHFileOperation(&fop))
                {
                    // could not delete file to the trashbin, so delete it directly
                    DeleteFile(it->first.c_str());
                }
            }
            else if (bCopyOnly)
            {
                std::wstring cryptpath = CPathUtils::AdjustForMaxPath(CPathUtils::Append(pt.cryptpath, it->first));
                std::wstring origpath = CPathUtils::AdjustForMaxPath(CPathUtils::Append(pt.origpath, it->first));
                CCircularLog::Instance()(_T("copy file %s to %s"), cryptpath.c_str(), origpath.c_str());
                // copy the file
                if (!CopyFile(cryptpath.c_str(), origpath.c_str(), FALSE))
                {
                    std::wstring targetfolder = pt.origpath + L"\\" + it->first;
                    targetfolder = targetfolder.substr(0, targetfolder.find_last_of('\\'));
                    SHCreateDirectory(NULL, targetfolder.c_str());
                    {
                        CAutoWriteLock nlocker(m_notignguard);
                        m_notifyignores.insert(CPathUtils::Append(pt.origpath, it->first));
                    }
                    CopyFile(cryptpath.c_str(), origpath.c_str(), FALSE);
                }
            }
            else
            {
                // decrypt the file
                CTraceToOutputDebugString::Instance()(_T(__FUNCTION__) _T(": decrypt file %s to %s\n"), it->first.c_str(), pt.origpath.c_str());
                std::wstring cryptpath = CPathUtils::AdjustForMaxPath(CPathUtils::Append(pt.cryptpath, it->second.filerelpath));
                std::wstring origpath = CPathUtils::AdjustForMaxPath(CPathUtils::Append(pt.origpath, it->first));
                if (!DecryptFile(origpath, cryptpath, pt.password, it->second, pt.useGPG))
                {
                    if (!it->second.filenameEncrypted)
                    {
                        {
                            CAutoWriteLock nlocker(m_notignguard);
                            m_notifyignores.insert(cryptpath);
                        }
                        MoveFileEx(cryptpath.c_str(), origpath.c_str(), MOVEFILE_COPY_ALLOWED);
                    }
                }
            }
        }
    }
    if (m_TrayWnd)
        PostMessage(m_TrayWnd, WM_PROGRESS, 0, 0);
}

std::map<std::wstring, FileData, ci_less> CFolderSync::GetFileList(bool orig, const std::wstring& path, const std::wstring& password, bool encnames, bool use7z, bool useGPG, DWORD & error) const
{
    error = 0;
    std::wstring enumpath = path;
    if ((enumpath.size() == 2)&&(enumpath[1]==':'))
        enumpath += L"\\";
    CDirFileEnum enumerator(enumpath);

    std::map<std::wstring,FileData, ci_less> filelist;
    std::wstring filepath;
    bool isDir = false;
    bool bRecurse = true;
    while (enumerator.NextFile(filepath, &isDir, bRecurse))
    {
        bRecurse = true;
        if (isDir)
        {
            if (CIgnores::Instance().IsIgnored(filepath))
                bRecurse = false;   // don't recurse into ignored folders
            continue;
        }
        if (!m_bRunning)
            break;

        FileData fd;

        fd.ft = enumerator.GetLastWriteTime();
        if ((fd.ft.dwLowDateTime == 0) && (fd.ft.dwHighDateTime == 0))
            fd.ft = enumerator.GetCreateTime();

        std::wstring relpath = filepath;
        if (path.size() < filepath.size())
            relpath = filepath.substr(path.size() + 1);
        fd.filerelpath = relpath;

        std::wstring decryptedRelPath = relpath;
        if (!orig)
            decryptedRelPath = GetDecryptedFilename(relpath, password, encnames, use7z, useGPG);
        fd.filenameEncrypted = (_wcsicmp(decryptedRelPath.c_str(), fd.filerelpath.c_str())!=0);
        if (fd.filenameEncrypted)
        {
            relpath = decryptedRelPath;
        }

        filelist[relpath] = fd;
        if (error == 0)
            error = enumerator.GetError();
    }
    return filelist;
}

bool CFolderSync::EncryptFile(const std::wstring& orig, const std::wstring& crypt, const std::wstring& password, const FileData& fd, bool useGPG)
{
    CTraceToOutputDebugString::Instance()(_T(__FUNCTION__) _T(": encrypt file %s to %s\n"), orig.c_str(), crypt.c_str());
    CCircularLog::Instance()(_T("encrypt file %s to %s"), orig.c_str(), crypt.c_str());

    size_t slashpos = crypt.find_last_of('\\');
    if (slashpos == std::string::npos)
        return false;
    std::wstring targetfolder = crypt.substr(0, slashpos);
    std::wstring cryptname = crypt.substr(slashpos+1);
    size_t buflen = orig.size() + crypt.size() + password.size() + 1000;
    std::unique_ptr<wchar_t[]> cmdlinebuf(new wchar_t[buflen]);
    if ((!cryptname.empty()) && (cryptname[0] == '-'))
        cryptname = L".\\" + cryptname;

    // try to open the source file in read mode:
    // if we can't open the file, then it is locked and 7-zip would fail.
    // But when 7-zip fails, it destroys a possible already existing encrypted file instead of
    // just leaving it as it is. So by first checking if the source file
    // can be read, we reduce the chances of 7-zip destroying the target file.
    CAutoFile hFile = CreateFile(orig.c_str(), GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE, NULL, OPEN_EXISTING, 0, NULL);
    if (!hFile.IsValid())
        return false;
    LARGE_INTEGER filesize = {0};
    GetFileSizeEx(hFile, &filesize);
    hFile.CloseHandle();

    // 7zip 9.30 has an option "-stl" which sets the timestamp of the archive
    // to the most recent one of the compressed files
    // add this flag as soon as 9.30 is stable and officially released.
    int compression = 9;
    if (filesize.QuadPart > 100*1024*1024)
        compression = 0;    // turn off compression for files bigger than 100MB

    if (password.empty())
    {
        CCircularLog::Instance()(_T("password is blank - NOT secure - force 7z not GPG"), crypt.c_str());
        swprintf_s(cmdlinebuf.get(), buflen, L"\"%s\" a -t7z -ssw \"%s\" \"%s\" -mx%d -mhe=on -m0=lzma2 -mtc=on -w", m_sevenzip.c_str(), cryptname.c_str(), orig.c_str(), compression);
    }
    else
    {

        if (useGPG)
            swprintf_s(cmdlinebuf.get(), buflen, L"\"%s\" --batch --yes -c -a --passphrase \"%s\" -o \"%s\" \"%s\" ", m_GnuPG.c_str(), password.c_str(), cryptname.c_str(), orig.c_str());
        else
            swprintf_s(cmdlinebuf.get(), buflen, L"\"%s\" a -t7z -ssw \"%s\" \"%s\" -p\"%s\" -mx%d -mhe=on -m0=lzma2 -mtc=on -w", m_sevenzip.c_str(), cryptname.c_str(), orig.c_str(), password.c_str(), compression);
    }

    bool bRet = RunExtTool(cmdlinebuf.get(), targetfolder, useGPG);
    if (!bRet)
    {
        SHCreateDirectory(NULL, targetfolder.c_str());
        {
            CAutoWriteLock nlocker(m_notignguard);
            m_notifyignores.insert(crypt);
        }
        bRet = RunExtTool(cmdlinebuf.get(), targetfolder, useGPG);
    }
    if (bRet)
    {
        // set the file timestamp
        int retry = 5;
        do
        {
            CAutoFile hFile = CreateFile(crypt.c_str(), GENERIC_WRITE|GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
            if (hFile.IsValid())
            {
                bRet = !!SetFileTime(hFile, NULL, NULL, &fd.ft);
            }
            else
                bRet = false;
            if (!bRet)
                Sleep(200);
        } while (!bRet && (retry-- > 0));
        if (!bRet)
            CCircularLog::Instance()(_T("failed to set file time on %s"), crypt.c_str());
        CAutoWriteLock locker(m_failureguard);
        m_failures.erase(orig);
    }
    else
    {
        // If encrypting failed, remove the leftover file
        DeleteFile(crypt.c_str());
        CAutoWriteLock locker(m_failureguard);
        m_failures[orig] = Encrypt;
        CCircularLog::Instance()(L"Failed to encrypt file \"%s\" to \"%s\"", orig.c_str(), crypt.c_str());
    }
    return bRet;
}

bool CFolderSync::DecryptFile( const std::wstring& orig, const std::wstring& crypt, const std::wstring& password, const FileData& fd, bool useGPG )
{
    CTraceToOutputDebugString::Instance()(_T(__FUNCTION__) _T(": decrypt file %s to %s\n"), crypt.c_str(), orig.c_str());
    CCircularLog::Instance()(_T("decrypt file %s to %s"), crypt.c_str(), orig.c_str());
    size_t slashpos = orig.find_last_of('\\');
    if (slashpos == std::string::npos)
        return false;
    std::wstring targetfolder = orig.substr(0, slashpos);
    size_t buflen = orig.size() + crypt.size() + password.size() + 1000;
    std::unique_ptr<wchar_t[]> cmdlinebuf(new wchar_t[buflen]);

    if (password.empty())
    {
        CCircularLog::Instance()(_T("password is blank - NOT secure - force 7z not GPG"), crypt.c_str());
        swprintf_s(cmdlinebuf.get(), buflen, L"\"%s\" e \"%s\" -o\"%s\" -y", m_sevenzip.c_str(), crypt.c_str(), targetfolder.c_str());
    }
    else
    {
        if (useGPG)
            swprintf_s(cmdlinebuf.get(), buflen, L"\"%s\" --yes --batch --passphrase \"%s\" -o \"%s\" \"%s\" ", m_GnuPG.c_str(), password.c_str(), orig.c_str(), crypt.c_str());
        else
            swprintf_s(cmdlinebuf.get(), buflen, L"\"%s\" e \"%s\" -o\"%s\" -p\"%s\" -y", m_sevenzip.c_str(), crypt.c_str(), targetfolder.c_str(), password.c_str());
    }
    bool bRet = RunExtTool(cmdlinebuf.get(), targetfolder, useGPG);
    if (!bRet)
    {
        SHCreateDirectory(NULL, targetfolder.c_str());
        {
            CAutoWriteLock nlocker(m_notignguard);
            m_notifyignores.insert(orig);
        }
        bRet = RunExtTool(cmdlinebuf.get(), targetfolder, useGPG);
    }
    if (bRet)
    {
        // set the file timestamp
        int retry = 5;
        do
        {
            CAutoFile hFile = CreateFile(orig.c_str(), GENERIC_WRITE|GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
            if (hFile.IsValid())
            {
                bRet = !!SetFileTime(hFile, NULL, NULL, &fd.ft);
            }
            else
                bRet = false;
            if (!bRet)
                Sleep(200);
        } while (!bRet && (retry-- > 0));
        if (!bRet)
            CCircularLog::Instance()(_T("failed to set file time on %s"), orig.c_str());
        CAutoWriteLock locker(m_failureguard);
        m_failures.erase(orig);
    }
    else
    {
        DeleteFile(orig.c_str());
        CAutoWriteLock locker(m_failureguard);
        m_failures[orig] = Decrypt;
        CCircularLog::Instance()(L"Failed to decrypt file \"%s\" to \"%s\"", crypt.c_str(), orig.c_str());
    }
    return bRet;
}

std::wstring CFolderSync::GetDecryptedFilename(const std::wstring& filename, const std::wstring& password, bool encryptname, bool use7z, bool useGPG)
{
    std::wstring decryptName = filename;
    size_t dotpos = filename.find_last_of('.');
    std::wstring fname;
    if (dotpos != std::string::npos)
        fname = filename.substr(0, dotpos);
    else
        fname = filename;

    if (!encryptname)
    {
        std::wstring f = filename;
        std::transform(f.begin(), f.end(), f.begin(), std::tolower);
        if (useGPG)
        {
            size_t pos = f.rfind(L".gpg");
            if ((pos != std::string::npos) && (pos == (filename.size() - 4)))
            {
                return filename.substr(0, pos);
            }
        }
        else
        {
            if (use7z)
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
    if (CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT|CRYPT_SILENT))
    {
        HCRYPTHASH hHash = NULL;
        // Create hash object.
        if (CryptCreateHash(hProv, CALG_MD5, 0, 0, &hHash))
        {
            // Hash password string.
            DWORD dwLength = DWORD(sizeof(WCHAR)*password.size());
            if (CryptHashData(hHash, (BYTE *)password.c_str(), dwLength, 0))
            {
                HCRYPTKEY hKey = NULL;
                // Create block cipher session key based on hash of the password.
                if (CryptDeriveKey(hProv, CALG_RC4, hHash, CRYPT_EXPORTABLE, &hKey))
                {
                    std::vector<std::wstring> names;
                    std::vector<std::wstring> decryptnames;
                    stringtok(names, fname, true, L"\\/");
                    for (auto it = names.cbegin(); it != names.cend(); ++it)
                    {
                        std::string name = CUnicodeUtils::StdGetUTF8(*it);
                        dwLength = DWORD(name.size() + 1024); // 1024 bytes should be enough for padding
                        std::unique_ptr<BYTE[]> buffer(new BYTE[dwLength]);

                        std::unique_ptr<BYTE[]> strIn(new BYTE[name.size()*sizeof(WCHAR) + 1]);
                        if (CStringUtils::FromHexString(name, strIn.get()))
                        {
                            // copy encrypted password to temporary buffer
                            memcpy(buffer.get(), strIn.get(), name.size());
                            CryptDecrypt(hKey, 0, true, 0, (BYTE *)buffer.get(), &dwLength);
                            decryptName = CUnicodeUtils::StdGetUnicode(std::string((char*)buffer.get(), name.size()/2));
                        }
                        else
                            decryptName = *it;
                        if (decryptName.empty() || (decryptName[0] != '*'))
                        {
                            if ((dotpos != std::string::npos)&&((it+1)==names.cend()))
                            {
                                std::wstring s = *it;
                                s += filename.substr(dotpos);
                                decryptnames.push_back(s);
                            }
                            else
                                decryptnames.push_back(*it);
                        }
                        else
                            decryptnames.push_back(decryptName.substr(1));    // cut off the starting '*'
                    }
                    CryptDestroyKey(hKey);  // Release provider handle.
                    decryptName.clear();
                    for (auto it = decryptnames.cbegin(); it != decryptnames.cend(); ++it)
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

std::wstring CFolderSync::GetEncryptedFilename(const std::wstring& filename, const std::wstring& password, bool encryptname, bool use7z, bool useGPG)
{
    std::wstring encryptFilename = filename;
    if (!encryptname)
    {
        std::wstring f = filename;
        std::transform(f.begin(), f.end(), f.begin(), std::tolower);

        if (useGPG)
        {
            encryptFilename += L".gpg";
            return encryptFilename;
        }
        else
        {
            if (use7z)
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

    bool bResult = true;

    HCRYPTPROV hProv = NULL;
    // Get handle to user default provider.
    if (CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT|CRYPT_SILENT))
    {
        HCRYPTHASH hHash = NULL;
        // Create hash object.
        if (CryptCreateHash(hProv, CALG_MD5, 0, 0, &hHash))
        {
            // Hash password string.
            DWORD dwLength = DWORD(sizeof(WCHAR)*password.size());
            if (CryptHashData(hHash, (BYTE *)password.c_str(), dwLength, 0))
            {
                // Create block cipher session key based on hash of the password.
                HCRYPTKEY hKey = NULL;
                if (CryptDeriveKey(hProv, CALG_RC4, hHash, CRYPT_EXPORTABLE, &hKey))
                {
                    std::vector<std::wstring> names;
                    std::vector<std::wstring> encryptnames;
                    stringtok(names, filename, true, L"\\/");
                    for (auto it = names.cbegin(); it != names.cend(); ++it)
                    {
                        // Determine number of bytes to encrypt at a time.
                        std::string starname = "*";
                        starname += CUnicodeUtils::StdGetUTF8(*it);

                        dwLength = (DWORD)starname.size();
                        std::unique_ptr<BYTE[]> buffer(new BYTE[dwLength+1024]);
                        memcpy(buffer.get(), starname.c_str(), dwLength);
                        // Encrypt data
                        if (CryptEncrypt(hKey, 0, true, 0, buffer.get(), &dwLength, dwLength+1024))
                        {
                            encryptFilename = CStringUtils::ToHexWString(buffer.get(), dwLength);
                            encryptnames.push_back(encryptFilename);
                        }
                        else
                        {
                            encryptnames.push_back(*it);
                            bResult = false;
                        }
                    }
                    encryptFilename.clear();
                    for (auto it = encryptnames.cbegin(); it != encryptnames.cend(); ++it)
                    {
                        if (!encryptFilename.empty())
                            encryptFilename += L"\\";
                        encryptFilename += *it;
                    }
                    if (useGPG)
                    {
                        encryptFilename += L".gpg";
                    }
                    else
                    {
                    if (use7z)
                        encryptFilename += L".7z";
                    else
                        encryptFilename += L".cryptsync";
                    }
                    CryptDestroyKey(hKey);  // Release provider handle.
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

bool CFolderSync::RunExtTool( LPWSTR cmdline, const std::wstring& cwd, bool useGPG ) const
{
    PROCESS_INFORMATION pi = {0};

    if (CCreateProcessHelper::CreateProcess(useGPG ? m_GnuPG.c_str() : m_sevenzip.c_str(), cmdline, cwd.c_str(), &pi, true, BELOW_NORMAL_PRIORITY_CLASS | CREATE_UNICODE_ENVIRONMENT))
    {
        // wait until the process terminates
        WaitForSingleObject(pi.hProcess, INFINITE);

        DWORD exitcode = 0;
        GetExitCodeProcess(pi.hProcess, &exitcode);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);

        return (exitcode == 0);
    }

    return false;
}

std::map<std::wstring, SyncOp> CFolderSync::GetFailures()
{
    CAutoReadLock locker(m_failureguard);
    return m_failures;
}

size_t CFolderSync::GetFailureCount()
{
    CAutoReadLock locker(m_failureguard);
    return m_failures.size();
}

std::set<std::wstring> CFolderSync::GetNotifyIgnores()
{
    CAutoWriteLock locker(m_notignguard);
    auto igncopy = m_notifyignores;
    m_notifyignores.clear();
    return igncopy;
}

std::wstring CFolderSync::GetFileTimeStringForLog( const FILETIME & ft )
{
    SYSTEMTIME stUTC, stLocal;
    FileTimeToSystemTime(&ft, &stUTC);
    SystemTimeToTzSpecificLocalTime(NULL, &stUTC, &stLocal);

    return CStringUtils::Format(L"%02d.%02d.%02d - %02d:%02d:%02d:%03d",
                                stLocal.wDay, stLocal.wMonth, stLocal.wYear,
                                stLocal.wHour, stLocal.wMinute, stLocal.wSecond, stLocal.wMilliseconds);
}
