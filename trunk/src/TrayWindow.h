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
#include "BaseWindow.h"
#include "resource.h"
#include "Registry.h"
#include "PathWatcher.h"

#include <shellapi.h>
#include <shlwapi.h>
#include <commctrl.h>


class CTrayWindow : public CWindow
{
public:
    CTrayWindow(HINSTANCE hInst, const WNDCLASSEX* wcx = NULL)
        : CWindow(hInst, wcx)
        , hwndNextViewer(NULL)
        , foregroundWND(NULL)
    {
        SetWindowTitle((LPCTSTR)ResString(hResource, IDS_APP_TITLE));
    };

    ~CTrayWindow(void)
    {
    };

    bool                RegisterAndCreateWindow();

protected:
    /// the message handler for this window
    LRESULT CALLBACK    WinMsgHandler(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    LRESULT             HandleCustomMessages(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    /// Handles all the WM_COMMAND window messages (e.g. menu commands)
    LRESULT             DoCommand(int id);

    void                ShowTrayIcon();
    DWORD               GetDllVersion(LPCTSTR lpszDllName);

protected:
    NOTIFYICONDATA      niData;
    HWND                hwndNextViewer;
    HWND                foregroundWND;
    CPathWatcher        watcher;

    typedef BOOL(__stdcall *PFNCHANGEWINDOWMESSAGEFILTEREX)(HWND hWnd, UINT message, DWORD dwFlag, PCHANGEFILTERSTRUCT pChangeFilterStruct);
    static PFNCHANGEWINDOWMESSAGEFILTEREX m_pChangeWindowMessageFilter;
};
