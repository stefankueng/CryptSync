// CryptSync - A folder sync tool with encryption

// Copyright (C) 2012-2014, 2021-2022 - Stefan Kueng

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

#include "BaseWindow.h"
#include "resource.h"
#include "PathWatcher.h"
#include "FolderSync.h"
#include "ResString.h"

#include <shellapi.h>
#include <shlwapi.h>

class CTrayWindow : public CWindow
{
public:
    CTrayWindow(HINSTANCE hInst, const WNDCLASSEX* wcx = nullptr)
        : CWindow(hInst, wcx)
        , m_niData{}
        , m_iconNormal(nullptr)
        , m_iconError(nullptr)
        , m_hwndNextViewer(nullptr)
        , m_foregroundWnd(nullptr)
        , m_bNewerVersionAvailable(false)
        , m_bTrayMode(true)
        , m_bOptionsDialogShown(false)
        , m_itemsProcessed(0)
        , m_totalItemsToProcess(0)
    {
        SecureZeroMemory(&m_niData, sizeof(m_niData));
        SetWindowTitle(static_cast<LPCTSTR>(ResString(hResource, IDS_APP_TITLE)));
    };

    ~CTrayWindow() override;
    ;

    bool RegisterAndCreateWindow();

    void ShowDialogImmediately(bool show) { m_bTrayMode = !show; }

protected:
    /// the message handler for this window
    LRESULT CALLBACK WinMsgHandler(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) override;
    LRESULT          HandleCustomMessages(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    /// Handles all the WM_COMMAND window messages (e.g. menu commands)
    LRESULT DoCommand(int id);

    void         ShowTrayIcon();
    static DWORD GetDllVersion(LPCTSTR lpszDllName);

    static unsigned int __stdcall UpdateCheckThreadEntry(void* pContext);
    void UpdateCheckThread();

protected:
    NOTIFYICONDATA         m_niData;
    HICON                  m_iconNormal;
    HICON                  m_iconError;
    HWND                   m_hwndNextViewer;
    HWND                   m_foregroundWnd;
    CPathWatcher           m_watcher;
    CFolderSync            m_folderSyncer;
    bool                   m_bNewerVersionAvailable;
    bool                   m_bTrayMode;
    bool                   m_bOptionsDialogShown;
    std::set<std::wstring> m_lastChangedPaths;
    int                    m_itemsProcessed;
    int                    m_totalItemsToProcess;

    typedef BOOL(__stdcall* PFNCHANGEWINDOWMESSAGEFILTEREX)(HWND hWnd, UINT message, DWORD dwFlag, PCHANGEFILTERSTRUCT pChangeFilterStruct);
    static PFNCHANGEWINDOWMESSAGEFILTEREX m_pChangeWindowMessageFilter;
};
