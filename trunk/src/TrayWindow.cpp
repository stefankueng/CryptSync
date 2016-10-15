// CryptSync - A folder sync tool with encryption

// Copyright (C) 2012-2014, 2016 - Stefan Kueng

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
#include "TrayWindow.h"
#include "AboutDlg.h"
#include "OptionsDlg.h"
#include "Ignores.h"
#include "UpdateDlg.h"
#include "DebugOutput.h"
#include "CircularLog.h"

#include <WindowsX.h>
#include <process.h>

#define TIMER_DETECTCHANGES 100
#define TIMER_DETECTCHANGESINTERVAL 10000
#define TIMER_FULLSCAN 101

UINT TIMER_FULLSCANINTERVAL = CRegStdDWORD(L"Software\\CryptSync\\FullScanInterval", 60000*30);

static UINT WM_TASKBARCREATED = RegisterWindowMessage(_T("TaskbarCreated"));
CTrayWindow::PFNCHANGEWINDOWMESSAGEFILTEREX CTrayWindow::m_pChangeWindowMessageFilter = NULL;

#define PACKVERSION(major,minor) MAKELONG(minor,major)

DWORD CTrayWindow::GetDllVersion(LPCTSTR lpszDllName)
{
    HINSTANCE hinstDll;
    DWORD dwVersion = 0;

    hinstDll = LoadLibrary(lpszDllName);

    if (hinstDll)
    {
        DLLGETVERSIONPROC pDllGetVersion;
        pDllGetVersion = (DLLGETVERSIONPROC)GetProcAddress(hinstDll,
            "DllGetVersion");

        if (pDllGetVersion)
        {
            DLLVERSIONINFO dvi;
            HRESULT hr;

            SecureZeroMemory(&dvi, sizeof(dvi));
            dvi.cbSize = sizeof(dvi);

            hr = (*pDllGetVersion)(&dvi);

            if (SUCCEEDED(hr))
            {
                dwVersion = PACKVERSION(dvi.dwMajorVersion, dvi.dwMinorVersion);
            }
        }

        FreeLibrary(hinstDll);
    }
    return dwVersion;
}

bool CTrayWindow::RegisterAndCreateWindow()
{
    WNDCLASSEX wcx;

    // Fill in the window class structure with default parameters
    wcx.cbSize = sizeof(WNDCLASSEX);
    wcx.style = CS_HREDRAW | CS_VREDRAW;
    wcx.lpfnWndProc = CWindow::stWinMsgHandler;
    wcx.cbClsExtra = 0;
    wcx.cbWndExtra = 0;
    wcx.hInstance = hResource;
    wcx.hCursor = NULL;
    ResString clsname(hResource, IDS_APP_TITLE);
    wcx.lpszClassName = clsname;
    wcx.hIcon = LoadIcon(hResource, MAKEINTRESOURCE(IDI_CryptSync));
    wcx.hbrBackground = NULL;
    wcx.lpszMenuName = NULL;
    wcx.hIconSm = LoadIcon(wcx.hInstance, MAKEINTRESOURCE(IDI_CryptSync));
    if (RegisterWindow(&wcx))
    {
        if (CreateEx(NULL, WS_POPUP, NULL))
        {
            // On Vista, the message TasbarCreated may be blocked by the message filter.
            // We try to change the filter here to get this message through. If even that
            // fails, then we can't do much about it and the task bar icon won't show up again.
            HMODULE hLib = LoadLibrary(_T("user32.dll"));
            if (hLib)
            {
                m_pChangeWindowMessageFilter = (CTrayWindow::PFNCHANGEWINDOWMESSAGEFILTEREX)GetProcAddress(hLib, "ChangeWindowMessageFilterEx");
                if (m_pChangeWindowMessageFilter)
                {
                    (*m_pChangeWindowMessageFilter)(m_hwnd, WM_TASKBARCREATED, MSGFLT_ALLOW, NULL);
                    (*m_pChangeWindowMessageFilter)(m_hwnd, WM_SETTINGCHANGE, MSGFLT_ALLOW, NULL);
                }
                FreeLibrary(hLib);
            }

            ShowTrayIcon();
            return true;
        }
    }
    return false;
}

void CTrayWindow::ShowTrayIcon()
{
    // since our main window is hidden most of the time
    // we have to add an auxiliary window to the system tray
    SecureZeroMemory(&niData, sizeof(niData));

    ULONGLONG ullVersion = GetDllVersion(_T("Shell32.dll"));
    if (ullVersion >= MAKEDLLVERULL(6,0,0,0))
        niData.cbSize = sizeof(NOTIFYICONDATA);
    else if(ullVersion >= MAKEDLLVERULL(5,0,0,0))
        niData.cbSize = NOTIFYICONDATA_V2_SIZE;
    else niData.cbSize = NOTIFYICONDATA_V1_SIZE;

    niData.uID = IDI_CryptSync;
    niData.uFlags = NIF_ICON|NIF_MESSAGE|NIF_TIP|NIF_INFO;

    niData.hIcon = (HICON)LoadImage(hResource, MAKEINTRESOURCE(IDI_CryptSync),
        IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);
    niData.hWnd = *this;
    niData.uCallbackMessage = TRAY_WM_MESSAGE;
    niData.uVersion = 6;

    Shell_NotifyIcon(NIM_DELETE,&niData);
    Shell_NotifyIcon(NIM_ADD,&niData);
    Shell_NotifyIcon(NIM_SETVERSION,&niData);
    DestroyIcon(niData.hIcon);
}

LRESULT CTrayWindow::HandleCustomMessages(HWND /*hwnd*/, UINT uMsg, WPARAM /*wParam*/, LPARAM /*lParam*/)
{
    if (uMsg == WM_TASKBARCREATED)
    {
        ShowTrayIcon();
    }
    return 0L;
}

LRESULT CALLBACK CTrayWindow::WinMsgHandler(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    // the custom messages are not constant, therefore we can't handle them in the
    // switch-case below
    HandleCustomMessages(hwnd, uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_CREATE:
        {
            m_hwnd = hwnd;
            watcher.ClearPaths();
            for (auto it = g_pairs.cbegin(); it != g_pairs.cend(); ++it)
            {
                std::wstring origpath = it->origpath;
                std::wstring cryptpath = it->cryptpath;
                if ((it->syncDir == BothWays) || (it->syncDir == SrcToDst))
                    watcher.AddPath(origpath);
                if ((it->syncDir == BothWays)||(it->syncDir == DstToSrc))
                    watcher.AddPath(cryptpath);
            }
            SetTimer(*this, TIMER_DETECTCHANGES, TIMER_DETECTCHANGESINTERVAL, NULL);
            SetTimer(*this, TIMER_FULLSCAN, TIMER_FULLSCANINTERVAL, NULL);
            unsigned int threadId = 0;
            _beginthreadex(NULL, 0, UpdateCheckThreadEntry, this, 0, &threadId);
            if (!m_bTrayMode)
                ::PostMessage(*this, WM_COMMAND, MAKEWPARAM(IDM_OPTIONS, 1), 0);
            foldersyncer.SetPairs(g_pairs);
            foldersyncer.SetTrayWnd(m_hwnd);
        }
        break;
    case WM_COMMAND:
        return DoCommand(LOWORD(wParam));
        break;
    case WM_PROGRESS:
        {
            m_itemsprocessed = (int)wParam;
            m_totalitemstoprocess = (int)lParam;
        }
        break;
    case TRAY_WM_MESSAGE:
        {
            switch(lParam)
            {
            case WM_RBUTTONUP:
            case WM_CONTEXTMENU:
                {
                    POINT pt;
                    GetCursorPos(&pt);
                    HMENU hMenu = LoadMenu(hResource, MAKEINTRESOURCE(IDC_CryptSync));
                    HMENU hPopMenu = GetSubMenu(hMenu, 0);
                    SetForegroundWindow(*this);
                    TrackPopupMenu(hPopMenu, TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, 0, *this, NULL);
                    DestroyMenu(hMenu);
                }
                break;
            case WM_LBUTTONDOWN:
                {
                    ::PostMessage(*this, WM_COMMAND, MAKEWPARAM(IDM_OPTIONS, 1), 0);
                }
                break;
            case WM_LBUTTONDBLCLK:
                {
                }
                break;
            case WM_MOUSEMOVE:
                {
                    int count = (int)foldersyncer.GetFailureCount();
                    WCHAR buf[200] = {0};
                    if (count)
                        swprintf_s(buf, L"%d items failed to synchronize", count);
                    else if (m_totalitemstoprocess)
                        swprintf_s(buf, L"Synched %d of %d items", m_itemsprocessed, m_totalitemstoprocess);
                    else
                        wcscpy_s(buf, L"synchronization ok");
                    niData.uFlags = NIF_TIP;
                    wcsncpy_s( niData.szTip, _countof( niData.szTip ), buf, _countof( niData.szTip ) );
                    Shell_NotifyIcon( NIM_MODIFY, &niData );
                }
                break;
            }
        }
        break;
    case WM_DESTROY:
        Shell_NotifyIcon(NIM_DELETE,&niData);
        PostQuitMessage(0);
        break;
    case WM_TIMER:
        switch (wParam)
        {
        case TIMER_DETECTCHANGES:
            {
                SetTimer(*this, TIMER_DETECTCHANGES, TIMER_DETECTCHANGESINTERVAL, NULL);
                if (!m_lastChangedPaths.empty())
                {
                    for (auto it = m_lastChangedPaths.cbegin(); it != m_lastChangedPaths.cend(); ++it)
                    {
                        if (CIgnores::Instance().IsIgnored(*it))
                            continue;

                        foldersyncer.SyncFile(*it);
                    }
                }
                m_lastChangedPaths = watcher.GetChangedPaths();
                auto ignores = foldersyncer.GetNotifyIgnores();
                if (!ignores.empty())
                {
                    for (auto it = ignores.cbegin(); it != ignores.end(); ++it)
                    {
                        auto foundIt = m_lastChangedPaths.find(*it);
                        if (foundIt != m_lastChangedPaths.end())
                        {
                            CTraceToOutputDebugString::Instance()(_T(__FUNCTION__) _T(": remove notification for file %s\n"), foundIt->c_str());
                            m_lastChangedPaths.erase(foundIt);
                        }
                    }
                }

                // if the number of watched paths is not what we expect,
                // reinitialize the paths: the watching may have failed
                // because the drive wasn't ready yet when CS started,
                // or even due to an access violation (virus scanners, ...)
                size_t watchpathcount = 0;
                for (auto it = g_pairs.cbegin(); it != g_pairs.cend(); ++it)
                {
                    if ((it->syncDir == BothWays) || (it->syncDir == DstToSrc))
                        ++watchpathcount;
                    if ((it->syncDir == BothWays) || (it->syncDir == SrcToDst))
                        ++watchpathcount;
                }

                if (watcher.GetNumberOfWatchedPaths() != watchpathcount)
                {
                    watcher.ClearPaths();
                    for (auto it = g_pairs.cbegin(); it != g_pairs.cend(); ++it)
                    {
                        std::wstring origpath = it->origpath;
                        std::wstring cryptpath = it->cryptpath;
                        if ((it->syncDir == BothWays) || (it->syncDir == SrcToDst))
                            watcher.AddPath(origpath);
                        if ((it->syncDir == BothWays) || (it->syncDir == DstToSrc))
                            watcher.AddPath(cryptpath);
                    }
                }
            }
            break;
        case TIMER_FULLSCAN:
            {
                // first handle the notifications
                for (int i = 0; i < 2; ++i)
                {
                    if (!m_lastChangedPaths.empty())
                    {
                        for (auto it = m_lastChangedPaths.cbegin(); it != m_lastChangedPaths.cend(); ++it)
                        {
                            if (CIgnores::Instance().IsIgnored(*it))
                                continue;

                            foldersyncer.SyncFile(*it);
                        }
                    }
                    m_lastChangedPaths = watcher.GetChangedPaths();
                    auto ignores = foldersyncer.GetNotifyIgnores();
                    if (!ignores.empty())
                    {
                        for (auto it = ignores.cbegin(); it != ignores.end(); ++it)
                        {
                            auto foundIt = m_lastChangedPaths.find(*it);
                            if (foundIt != m_lastChangedPaths.end())
                            {
                                CTraceToOutputDebugString::Instance()(_T(__FUNCTION__) _T(": remove notification for file %s\n"), foundIt->c_str());
                                m_lastChangedPaths.erase(foundIt);
                            }
                        }
                    }
                }
                // now start the full scan
                foldersyncer.SyncFolders(g_pairs);
                watcher.ClearPaths();
                for (auto it = g_pairs.cbegin(); it != g_pairs.cend(); ++it)
                {
                    std::wstring origpath = it->origpath;
                    std::wstring cryptpath = it->cryptpath;
                    if ((it->syncDir == BothWays) || (it->syncDir == SrcToDst))
                        watcher.AddPath(origpath);
                    if ((it->syncDir == BothWays) || (it->syncDir == DstToSrc))
                        watcher.AddPath(cryptpath);
                }
                SetTimer(*this, TIMER_FULLSCAN, TIMER_FULLSCANINTERVAL, NULL);
            }
            break;
        }
        break;
    case WM_QUERYENDSESSION:
        foldersyncer.Stop();
        watcher.Stop();
        return TRUE;
    case WM_CLOSE:
    case WM_ENDSESSION:
    case WM_QUIT:
        foldersyncer.Stop();
        watcher.Stop();
        ::PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }

    return 0;
};

LRESULT CTrayWindow::DoCommand(int id)
{
    switch (id)
    {
    case IDM_EXIT:
        Shell_NotifyIcon(NIM_DELETE,&niData);
        watcher.Stop();
        ::PostQuitMessage(0);
        return 0;
        break;
    case IDM_ABOUT:
        {
            CAboutDlg dlg(NULL);
            dlg.DoModal(hResource, IDD_ABOUTBOX, NULL);
        }
        break;
    case IDM_OPTIONS:
        {
            if (m_bOptionsDialogShown)
                break;
            m_bOptionsDialogShown = true;
            COptionsDlg dlg(NULL);
            dlg.SetUpdateAvailable(m_bNewerVersionAvailable);
            dlg.SetFailures(foldersyncer.GetFailures());
            INT_PTR ret = dlg.DoModal(hResource, IDD_OPTIONS, NULL);
            m_bOptionsDialogShown = false;
            if ((ret == IDOK)||(ret == IDCANCEL))
            {
                TIMER_FULLSCANINTERVAL = CRegStdDWORD(L"Software\\CryptSync\\FullScanInterval", 60000*30);
                foldersyncer.SyncFolders(g_pairs);
                watcher.ClearPaths();
                for (auto it = g_pairs.cbegin(); it != g_pairs.cend(); ++it)
                {
                    std::wstring origpath = it->origpath;
                    std::wstring cryptpath = it->cryptpath;
                    if ((it->syncDir == BothWays) || (it->syncDir == SrcToDst))
                        watcher.AddPath(origpath);
                    if ((it->syncDir == BothWays) || (it->syncDir == DstToSrc))
                        watcher.AddPath(cryptpath);
                }
                SetTimer(*this, TIMER_DETECTCHANGES, TIMER_DETECTCHANGESINTERVAL, NULL);
                SetTimer(*this, TIMER_FULLSCAN, TIMER_FULLSCANINTERVAL, NULL);
            }
            else
            {
                Shell_NotifyIcon(NIM_DELETE,&niData);
                watcher.Stop();
                ::PostQuitMessage(0);
                return 0;
            }
        }
        break;
    case ID_FILE_SYNCNOW:
        SetTimer(*this, TIMER_FULLSCAN, 1, NULL);
        break;
    default:
        break;
    };
    return 1;
}

unsigned int CTrayWindow::UpdateCheckThreadEntry(void* pContext)
{
    ((CTrayWindow*)pContext)->UpdateCheckThread();
    _endthreadex(0);
    return 0;
}

void CTrayWindow::UpdateCheckThread()
{
    m_bNewerVersionAvailable = CUpdateDlg::CheckNewer();
    if (m_bNewerVersionAvailable && m_bTrayMode)
    {
        CUpdateDlg dlg(NULL);
        dlg.DoModal(hResource, IDD_NEWERNOTIFYDLG, NULL);
        m_bNewerVersionAvailable = false;
    }
}
