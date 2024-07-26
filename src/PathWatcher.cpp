// CryptSync - A folder sync tool with encryption

// Copyright (C) 2012-2016, 2021, 2024 - Stefan Kueng

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
#include "PathWatcher.h"
#include "DebugOutput.h"
#include "PathUtils.h"

#include <Dbt.h>
#include <process.h>
#ifdef _DEBUG
#include <comdef.h>
#endif

CPathWatcher::CPathWatcher()
    : m_hCompPort(nullptr)
    , m_bRunning(TRUE)
{
    // enable the required privileges for this process

    LPCTSTR arPrivelegeNames[] = {SE_BACKUP_NAME,
                                  SE_RESTORE_NAME,
                                  SE_CHANGE_NOTIFY_NAME};

    for (int i = 0; i < _countof(arPrivelegeNames); ++i)
    {
        CAutoGeneralHandle hToken;
        if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, hToken.GetPointer()))
        {
            TOKEN_PRIVILEGES tp = {1};

            if (LookupPrivilegeValue(nullptr, arPrivelegeNames[i], &tp.Privileges[0].Luid))
            {
                tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

                AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(tp), nullptr, nullptr);
            }
        }
    }

    unsigned int threadId = 0;
    m_hThread             = reinterpret_cast<HANDLE>(_beginthreadex(nullptr, 0, ThreadEntry, this, 0, &threadId));
}

CPathWatcher::~CPathWatcher()
{
    Stop();
    CAutoWriteLock locker(m_guard);
    ClearInfoMap();
}

void CPathWatcher::Stop()
{
    InterlockedExchange(&m_bRunning, FALSE);
    if (m_hCompPort)
    {
        PostQueuedCompletionStatus(m_hCompPort, 0, NULL, nullptr);
        m_hCompPort.CloseHandle();
    }

    if (m_hThread)
    {
        // the background thread sleeps for 200ms,
        // so lets wait for it to finish for 1000 ms.

        WaitForSingleObject(m_hThread, 1000);
        m_hThread.CloseHandle();
    }
}

bool CPathWatcher::RemovePath(const std::wstring& path)
{
    CAutoWriteLock locker(m_guard);

    CTraceToOutputDebugString::Instance()(_T(__FUNCTION__) _T(": RemovePath for %s\n"), path.c_str());
    bool bRet = (watchedPaths.erase(path) != 0);
    m_hCompPort.CloseHandle();
    return bRet;
}

bool CPathWatcher::AddPath(const std::wstring& path)
{
    CAutoWriteLock locker(m_guard);
    CTraceToOutputDebugString::Instance()(_T(__FUNCTION__) _T(": AddPath for %s\n"), path.c_str());
    watchedPaths.insert(path);
    m_hCompPort.CloseHandle();
    return true;
}

unsigned int CPathWatcher::ThreadEntry(void* pContext)
{
    static_cast<CPathWatcher*>(pContext)->WorkerThread();
    return 0;
}

void CPathWatcher::WorkerThread()
{
    DWORD          lasterr;
    DWORD          numBytes;
    CDirWatchInfo* pdi = nullptr;
    LPOVERLAPPED   lpOverlapped;
    while (m_bRunning)
    {
        if (!watchedPaths.empty())
        {
            if (!m_hCompPort || !GetQueuedCompletionStatus(m_hCompPort,
                                                           &numBytes,
                                                           reinterpret_cast<PULONG_PTR>(&pdi),
                                                           &lpOverlapped,
                                                           INFINITE))
            {
                // Error retrieving changes
                // Clear the list of watched objects and recreate that list
                if (!m_bRunning)
                    return;

                lasterr = GetLastError();

                {
                    CAutoWriteLock locker(m_guard);
                    ClearInfoMap();
                }
#ifdef _DEBUG
                if (m_hCompPort)
                {
                    _com_error comError(lasterr);
                    LPCTSTR    comErrorText = comError.ErrorMessage();
                    CTraceToOutputDebugString::Instance()(_T(__FUNCTION__) _T(": GetQueuedCompletionStatus (%s) \n"), comErrorText);
                }
#endif
                if ((m_hCompPort) && (lasterr != ERROR_SUCCESS) && (lasterr != ERROR_INVALID_HANDLE))
                {
                    m_hCompPort.CloseHandle();
                }
                CAutoReadLock locker(m_guard);
                for (auto p = watchedPaths.cbegin(); p != watchedPaths.cend(); ++p)
                {
                    CAutoFile hDir = CreateFile(CPathUtils::AdjustForMaxPath(p->c_str()).c_str(),
                                                FILE_LIST_DIRECTORY,
                                                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                                nullptr, // security attributes
                                                OPEN_EXISTING,
                                                FILE_FLAG_BACKUP_SEMANTICS | // required privileges: SE_BACKUP_NAME and SE_RESTORE_NAME.
                                                    FILE_FLAG_OVERLAPPED,
                                                nullptr);
                    if (!hDir)
                    {
                        // this could happen if a watched folder has been removed/renamed
                        m_hCompPort.CloseHandle();
                        CAutoWriteLock lockerW(m_guard);
                        watchedPaths.erase(p);
                        break;
                    }

                    auto pDirInfo = std::make_unique<CDirWatchInfo>(std::move(hDir), p->c_str());
                    m_hCompPort   = CreateIoCompletionPort(pDirInfo->m_hDir, m_hCompPort, reinterpret_cast<ULONG_PTR>(pDirInfo.get()), 0);
                    if (m_hCompPort == NULL)
                    {
                        CAutoWriteLock lockerW(m_guard);
                        ClearInfoMap();
                        watchedPaths.erase(p);
                        break;
                    }
                    SecureZeroMemory(pDirInfo->m_buffer, sizeof(pDirInfo->m_buffer));
                    SecureZeroMemory(&pDirInfo->m_overlapped, sizeof(pDirInfo->m_overlapped));
                    if (!ReadDirectoryChangesW(pDirInfo->m_hDir,
                                               pDirInfo->m_buffer,
                                               READ_DIR_CHANGE_BUFFER_SIZE,
                                               TRUE,
                                               FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE,
                                               &numBytes, // not used
                                               &pDirInfo->m_overlapped,
                                               nullptr)) // no completion routine!
                    {
                        CAutoWriteLock lockerW(m_guard);
                        ClearInfoMap();
                        watchedPaths.erase(p);
                        break;
                    }
                    CTraceToOutputDebugString::Instance()(_T(__FUNCTION__) _T(": watching path %s\n"), p->c_str());
                    CAutoWriteLock lockerW(m_guard);
                    m_watchInfoMap[pDirInfo->m_hDir] = pDirInfo.get();
                    pDirInfo.release();
                }
            }
            else
            {
                if (!m_bRunning)
                    return;
                // NOTE: the longer this code takes to execute until ReadDirectoryChangesW
                // is called again, the higher the chance that we miss some
                // changes in the file system!
                if (pdi)
                {
                    if (numBytes != 0)
                    {
                        PFILE_NOTIFY_INFORMATION pnotify = reinterpret_cast<PFILE_NOTIFY_INFORMATION>(pdi->m_buffer);
                        DWORD                    nOffset;
                        do
                        {
                            size_t bufferSize = pdi->m_dirPath.size() + (pnotify->FileNameLength / sizeof(pnotify->FileName[0])) + 1;
                            auto   buf        = std::make_unique<wchar_t[]>(bufferSize);
                            nOffset           = pnotify->NextEntryOffset;
                            auto action       = pnotify->Action;

                            if (reinterpret_cast<ULONG_PTR>(pnotify) - reinterpret_cast<ULONG_PTR>(pdi->m_buffer) > READ_DIR_CHANGE_BUFFER_SIZE)
                                break;

                            wcscpy_s(buf.get(), bufferSize, pdi->m_dirPath.c_str());

                            // pnotify->FileName is not null terminated, the second argument to wcsncat_s limits the number of characters
                            // concatenated and the last parameter forces truncation; STRUNCATE, the expected return value since buf is allocated
                            // accordingly, is a valid return value.
                            // errno_t err     = wcsncat_s(buf.get() + pdi->m_dirPath.size(), min(pnotify->FileNameLength / sizeof(pnotify->FileName[0]) + 1, bufferSize - pdi->m_dirPath.size()), pnotify->FileName, _TRUNCATE);
                            // Above code may do a one-wchar_t source buffer read overrun on m_buffer, we can either declare m_buffer as "READ_DIR_CHANGE_BUFFER_SIZE + sizeof(wchar_t)" bytes
                            // or use memmove_s() to prevent source buffer overrun

                            errno_t err         = wmemmove_s(buf.get() + pdi->m_dirPath.size(),
                                                             min(pnotify->FileNameLength / sizeof(pnotify->FileName[0]), bufferSize - pdi->m_dirPath.size()),
                                                             pnotify->FileName,
                                                             pnotify->FileNameLength / sizeof(pnotify->FileName[0]));

                            buf[bufferSize - 1] = 0;
                            pnotify = reinterpret_cast<PFILE_NOTIFY_INFORMATION>(reinterpret_cast<LPBYTE>(pnotify) + nOffset);
                            if (err != 0)
                            {
                                continue;
                            }
                            CTraceToOutputDebugString::Instance()(_T(__FUNCTION__) _T(": change notification for %s (Action:%d)\n"), buf.get(), action);
                            {
                                CAutoWriteLock locker(m_guard);
                                m_changedPaths.insert(std::wstring(buf.get()));
                            }
                        } while (nOffset);
                    }
                    else
                    {
#ifdef _DEBUG
                        if (m_hCompPort)
                        {
                            lasterr = GetLastError();
                            _com_error comError(lasterr);
                            LPCTSTR    comErrorText = comError.ErrorMessage();
                            CTraceToOutputDebugString::Instance()(_T(__FUNCTION__) _T(": GetQueuedCompletionStatus returned zero numBytes (%s) for watched folder \"%s\"\n"), comErrorText, pdi->m_dirPath.c_str());
                        }
#endif
                    }
                    SecureZeroMemory(pdi->m_buffer, sizeof(pdi->m_buffer));
                    SecureZeroMemory(&pdi->m_overlapped, sizeof(pdi->m_overlapped));
                    if (!ReadDirectoryChangesW(pdi->m_hDir,
                                               pdi->m_buffer,
                                               READ_DIR_CHANGE_BUFFER_SIZE,
                                               TRUE,
                                               FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE,
                                               /*
                                                    Warning: including FILE_NOTIFY_CHANGE_ATTRIBUTES below would
                                                             result in notifications when we change the "index"
                                                             on target file or "archive" on source
                                               */
                                               &numBytes, // not used
                                               &pdi->m_overlapped,
                                               nullptr)) // no completion routine!
                    {
                        // Since the call to ReadDirectoryChangesW failed, just
                        // wait a while. We don't want to have this thread
                        // running using 100% CPU if something goes completely
                        // wrong.
                        Sleep(200);
                    }
                }
            }
        } // if (watchedPaths.GetCount())
        else
            Sleep(200);
    } // while (m_bRunning)
}

void CPathWatcher::ClearInfoMap()
{
    if (!m_watchInfoMap.empty())
    {
        CAutoWriteLock locker(m_guard);
        for (std::map<HANDLE, CDirWatchInfo*>::iterator I = m_watchInfoMap.begin(); I != m_watchInfoMap.end(); ++I)
        {
            CPathWatcher::CDirWatchInfo* info = I->second;
            delete info;
            info = nullptr;
        }
    }
    m_watchInfoMap.clear();
    m_hCompPort.CloseHandle();
}

std::set<std::wstring> CPathWatcher::GetChangedPaths()
{
    CAutoWriteLock         locker(m_guard);
    std::set<std::wstring> ret = m_changedPaths;
    m_changedPaths.clear();
    return ret;
}

CPathWatcher::CDirWatchInfo::CDirWatchInfo(CAutoFile&& hDir, const std::wstring& directoryName)
    : m_hDir(std::move(hDir))
    , m_dirName(directoryName)
{
    reinterpret_cast<PFILE_NOTIFY_INFORMATION>(m_buffer)->NextEntryOffset = 0;
    SecureZeroMemory(&m_overlapped, sizeof(m_overlapped));
    m_dirPath = m_dirName;
    if (m_dirPath.at(m_dirPath.size() - 1) != '\\')
        m_dirPath += _T("\\");
}

CPathWatcher::CDirWatchInfo::~CDirWatchInfo()
{
    CloseDirectoryHandle();
}

bool CPathWatcher::CDirWatchInfo::CloseDirectoryHandle()
{
    return m_hDir.CloseHandle();
}
