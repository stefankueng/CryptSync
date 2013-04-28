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
#include "stdafx.h"
#include "PathWatcher.h"
#include "DebugOutput.h"

#include <Dbt.h>
#include <process.h>


CPathWatcher::CPathWatcher(void)
    : m_hCompPort(NULL)
    , m_bRunning(TRUE)
{
    // enable the required privileges for this process

    LPCTSTR arPrivelegeNames[] = {  SE_BACKUP_NAME,
                                    SE_RESTORE_NAME,
                                    SE_CHANGE_NOTIFY_NAME
                                 };

    for (int i=0; i<(sizeof(arPrivelegeNames)/sizeof(LPCTSTR)); ++i)
    {
        CAutoGeneralHandle hToken;
        if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, hToken.GetPointer()))
        {
            TOKEN_PRIVILEGES tp = { 1 };

            if (LookupPrivilegeValue(NULL, arPrivelegeNames[i],  &tp.Privileges[0].Luid))
            {
                tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

                AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(tp), NULL, NULL);
            }
        }
    }

    unsigned int threadId = 0;
    m_hThread = (HANDLE)_beginthreadex(NULL,0,ThreadEntry,this,0,&threadId);
}

CPathWatcher::~CPathWatcher(void)
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
        PostQueuedCompletionStatus(m_hCompPort, 0, NULL, NULL);
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
    ((CPathWatcher*)pContext)->WorkerThread();
    return 0;
}

void CPathWatcher::WorkerThread()
{
    DWORD numBytes;
    CDirWatchInfo * pdi = NULL;
    LPOVERLAPPED lpOverlapped;
    const int bufferSize = MAX_PATH * 4;
    TCHAR buf[bufferSize] = {0};
    while (m_bRunning)
    {
        if (!watchedPaths.empty())
        {
            if (!m_hCompPort || !GetQueuedCompletionStatus(m_hCompPort,
                                                           &numBytes,
                                                           (PULONG_PTR) &pdi,
                                                           &lpOverlapped,
                                                           INFINITE))
            {
                // Error retrieving changes
                // Clear the list of watched objects and recreate that list
                if (!m_bRunning)
                    return;
                {
                    CAutoWriteLock locker(m_guard);
                    ClearInfoMap();
                }
                DWORD lasterr = GetLastError();
                if ((m_hCompPort)&&(lasterr!=ERROR_SUCCESS)&&(lasterr!=ERROR_INVALID_HANDLE))
                {
                    m_hCompPort.CloseHandle();
                }
                CAutoReadLock locker(m_guard);
                for (auto p = watchedPaths.cbegin(); p != watchedPaths.cend(); ++p)
                {
                    CAutoFile hDir = CreateFile(p->c_str(),
                                                FILE_LIST_DIRECTORY,
                                                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                                NULL, //security attributes
                                                OPEN_EXISTING,
                                                FILE_FLAG_BACKUP_SEMANTICS | //required privileges: SE_BACKUP_NAME and SE_RESTORE_NAME.
                                                FILE_FLAG_OVERLAPPED,
                                                NULL);
                    if (!hDir)
                    {
                        // this could happen if a watched folder has been removed/renamed
                        m_hCompPort.CloseHandle();
                        CAutoWriteLock locker(m_guard);
                        watchedPaths.erase(p);
                        break;
                    }

                    std::unique_ptr<CDirWatchInfo> pDirInfo (new CDirWatchInfo(hDir, p->c_str()));
                    hDir.Detach();  // the new CDirWatchInfo object owns the handle now
                    m_hCompPort = CreateIoCompletionPort(pDirInfo->m_hDir, m_hCompPort, (ULONG_PTR)pDirInfo.get(), 0);
                    if (m_hCompPort == NULL)
                    {
                        CAutoWriteLock locker(m_guard);
                        ClearInfoMap();
                        watchedPaths.erase(p);
                        break;
                    }
                    if (!ReadDirectoryChangesW(pDirInfo->m_hDir,
                                                pDirInfo->m_Buffer,
                                                READ_DIR_CHANGE_BUFFER_SIZE,
                                                TRUE,
                                                FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE,
                                                &numBytes,// not used
                                                &pDirInfo->m_Overlapped,
                                                NULL))  //no completion routine!
                    {
                        CAutoWriteLock locker(m_guard);
                        ClearInfoMap();
                        watchedPaths.erase(p);
                        break;
                    }
                    CTraceToOutputDebugString::Instance()(_T(__FUNCTION__) _T(": watching path %s\n"), p->c_str());
                    CAutoWriteLock locker(m_guard);
                    watchInfoMap[pDirInfo->m_hDir] = pDirInfo.get();
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
                    if (numBytes == 0)
                    {
                        goto continuewatching;
                    }
                    PFILE_NOTIFY_INFORMATION pnotify = (PFILE_NOTIFY_INFORMATION)pdi->m_Buffer;
                    if ((ULONG_PTR)pnotify - (ULONG_PTR)pdi->m_Buffer > READ_DIR_CHANGE_BUFFER_SIZE)
                        goto continuewatching;
                    DWORD nOffset = pnotify->NextEntryOffset;
                    do
                    {
                        nOffset = pnotify->NextEntryOffset;
                        SecureZeroMemory(buf, bufferSize*sizeof(TCHAR));
                        wcsncpy_s(buf, bufferSize, pdi->m_DirPath.c_str(), bufferSize);
                        errno_t err = wcsncat_s(buf+pdi->m_DirPath.size(), bufferSize-pdi->m_DirPath.size(), pnotify->FileName, min(pnotify->FileNameLength/sizeof(WCHAR), bufferSize-pdi->m_DirPath.size()));
                        if (err == STRUNCATE)
                        {
                            pnotify = (PFILE_NOTIFY_INFORMATION)((LPBYTE)pnotify + nOffset);
                            continue;
                        }
                        buf[min(bufferSize-1, pdi->m_DirPath.size()+(pnotify->FileNameLength/sizeof(WCHAR)))] = 0;
                        pnotify = (PFILE_NOTIFY_INFORMATION)((LPBYTE)pnotify + nOffset);
                        CTraceToOutputDebugString::Instance()(_T(__FUNCTION__) _T(": change notification for %s\n"), buf);
                        {
                            CAutoWriteLock locker(m_guard);
                            m_changedPaths.insert(std::wstring(buf));
                        }
                        if ((ULONG_PTR)pnotify - (ULONG_PTR)pdi->m_Buffer > READ_DIR_CHANGE_BUFFER_SIZE)
                            break;
                    } while (nOffset);
continuewatching:
                    SecureZeroMemory(pdi->m_Buffer, sizeof(pdi->m_Buffer));
                    SecureZeroMemory(&pdi->m_Overlapped, sizeof(OVERLAPPED));
                    if (!ReadDirectoryChangesW(pdi->m_hDir,
                                                pdi->m_Buffer,
                                                READ_DIR_CHANGE_BUFFER_SIZE,
                                                TRUE,
                                                FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE,
                                                &numBytes,// not used
                                                &pdi->m_Overlapped,
                                                NULL))  //no completion routine!
                    {
                        // Since the call to ReadDirectoryChangesW failed, just
                        // wait a while. We don't want to have this thread
                        // running using 100% CPU if something goes completely
                        // wrong.
                        Sleep(200);
                    }
                }
            }
        }// if (watchedPaths.GetCount())
        else
            Sleep(200);
    }// while (m_bRunning)
}

void CPathWatcher::ClearInfoMap()
{
    if (!watchInfoMap.empty())
    {
        CAutoWriteLock locker(m_guard);
        for (std::map<HANDLE, CDirWatchInfo *>::iterator I = watchInfoMap.begin(); I != watchInfoMap.end(); ++I)
        {
            CPathWatcher::CDirWatchInfo * info = I->second;
            delete info;
            info = NULL;
        }
    }
    watchInfoMap.clear();
    m_hCompPort.CloseHandle();
}

std::set<std::wstring> CPathWatcher::GetChangedPaths()
{
    CAutoWriteLock locker(m_guard);
    std::set<std::wstring> ret = m_changedPaths;
    m_changedPaths.clear();
    return ret;
}

CPathWatcher::CDirWatchInfo::CDirWatchInfo(HANDLE hDir, const std::wstring& DirectoryName)
    : m_hDir(hDir)
    , m_DirName(DirectoryName)
{
    m_Buffer[0] = 0;
    memset(&m_Overlapped, 0, sizeof(m_Overlapped));
    m_DirPath = m_DirName.c_str();
    if (m_DirPath.at(m_DirPath.size()-1) != '\\')
        m_DirPath += _T("\\");
}

CPathWatcher::CDirWatchInfo::~CDirWatchInfo()
{
    CloseDirectoryHandle();
}

bool CPathWatcher::CDirWatchInfo::CloseDirectoryHandle()
{
    return m_hDir.CloseHandle();
}
