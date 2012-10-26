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
#include "ReaderWriterLock.h"
#include "SmartHandle.h"
#include <string>
#include <set>
#include <map>

#define READ_DIR_CHANGE_BUFFER_SIZE 4096
#define MAX_CHANGED_PATHS  4000


/**
 * \ingroup Utils
 * Watches the file system for changes.
 *
 * When a CPathWatcher object is created, a new thread is started which
 * waits for file system change notifications.
 * To add folders to the list of watched folders, call \c AddPath().
 *
 * The folders are watched recursively. To prevent having too many folders watched,
 * children of already watched folders are automatically removed from watching.
 */
class CPathWatcher
{
public:
    CPathWatcher(void);
    ~CPathWatcher(void);

    /**
     * Adds a new path to be watched. The path \b must point to a directory.
     * If the path is already watched because a parent of that path is already
     * watched recursively, then the new path is just ignored and the method
     * returns false.
     */
    bool AddPath(const std::wstring& path);
    /**
     * Removes a path and all its children from the watched list.
     */
    bool RemovePath(const std::wstring& path);

    /**
     * Removes all watched paths
     */
    void ClearPaths() { CAutoWriteLock locker(m_guard); watchedPaths.clear(); m_hCompPort.CloseHandle(); }

    /**
     * Returns the number of recursively watched paths.
     */
    size_t GetNumberOfWatchedPaths() {return watchedPaths.size();}

    /**
     * Stops the watching thread.
     */
    void Stop();


private:
    static unsigned int __stdcall ThreadEntry(void* pContext);
    void WorkerThread();

    void ClearInfoMap();

private:
    CReaderWriterLock       m_guard;
    CAutoGeneralHandle      m_hThread;
    CAutoGeneralHandle      m_hCompPort;
    volatile LONG           m_bRunning;

    std::set<std::wstring>  watchedPaths;   ///< list of watched paths.

    /**
     * Helper class: provides information about watched directories.
     */
    class CDirWatchInfo
    {
    private:
        CDirWatchInfo();    // private & not implemented
        CDirWatchInfo & operator=(const CDirWatchInfo & rhs);//so that they're aren't accidentally used. -- you'll get a linker error
    public:
        CDirWatchInfo(HANDLE hDir, const std::wstring& DirectoryName);
        ~CDirWatchInfo();

    protected:
    public:
        bool            CloseDirectoryHandle();

        CAutoFile       m_hDir;         ///< handle to the directory that we're watching
        std::wstring    m_DirName;      ///< the directory that we're watching
        CHAR            m_Buffer[READ_DIR_CHANGE_BUFFER_SIZE]; ///< buffer for ReadDirectoryChangesW
        OVERLAPPED      m_Overlapped;
        std::wstring    m_DirPath;      ///< the directory name we're watching with a backslash at the end
    };

    std::map<HANDLE, CDirWatchInfo *> watchInfoMap;

    HDEVNOTIFY      m_hdev;

};
