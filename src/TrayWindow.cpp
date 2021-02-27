// CryptSync - A folder sync tool with encryption

// Copyright (C) 2012-2014, 2016, 2021 - Stefan Kueng

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
#include "ResString.h"

#include <WindowsX.h>
#include <process.h>

constexpr auto TIMER_DETECTCHANGES         = 100;
constexpr auto TIMER_DETECTCHANGESINTERVAL = 10000;
constexpr auto TIMER_FULLSCAN              = 101;

UINT TIMER_FULLSCANINTERVAL = CRegStdDWORD(L"Software\\CryptSync\\FullScanInterval", 60000 * 30);

static UINT                                 WM_TASKBARCREATED                         = RegisterWindowMessage(_T("TaskbarCreated"));
CTrayWindow::PFNCHANGEWINDOWMESSAGEFILTEREX CTrayWindow::m_pChangeWindowMessageFilter = nullptr;

#define PACKVERSION(major, minor) MAKELONG(minor, major)

DWORD CTrayWindow::GetDllVersion(LPCTSTR lpszDllName)
{
    DWORD dwVersion = 0;

    HINSTANCE hInstDll = LoadLibrary(lpszDllName);

    if (hInstDll)
    {
        DLLGETVERSIONPROC pDllGetVersion = reinterpret_cast<DLLGETVERSIONPROC>(GetProcAddress(hInstDll,
                                                                                              "DllGetVersion"));

        if (pDllGetVersion)
        {
            DLLVERSIONINFO dvi;

            SecureZeroMemory(&dvi, sizeof(dvi));
            dvi.cbSize = sizeof(dvi);

            HRESULT hr = (*pDllGetVersion)(&dvi);

            if (SUCCEEDED(hr))
            {
                dwVersion = PACKVERSION(dvi.dwMajorVersion, dvi.dwMinorVersion);
            }
        }

        FreeLibrary(hInstDll);
    }
    return dwVersion;
}

bool CTrayWindow::RegisterAndCreateWindow()
{
    WNDCLASSEX wcx;

    // Fill in the window class structure with default parameters
    wcx.cbSize      = sizeof(WNDCLASSEX);
    wcx.style       = CS_HREDRAW | CS_VREDRAW;
    wcx.lpfnWndProc = CWindow::stWinMsgHandler;
    wcx.cbClsExtra  = 0;
    wcx.cbWndExtra  = 0;
    wcx.hInstance   = hResource;
    wcx.hCursor     = nullptr;
    ResString clsName(hResource, IDS_APP_TITLE);
    wcx.lpszClassName = clsName;
    wcx.hIcon         = LoadIcon(hResource, MAKEINTRESOURCE(IDI_CryptSync));
    wcx.hbrBackground = nullptr;
    wcx.lpszMenuName  = nullptr;
    wcx.hIconSm       = LoadIcon(wcx.hInstance, MAKEINTRESOURCE(IDI_CryptSync));
    if (RegisterWindow(&wcx))
    {
        if (CreateEx(NULL, WS_POPUP, nullptr))
        {
            // On Vista, the message TasbarCreated may be blocked by the message filter.
            // We try to change the filter here to get this message through. If even that
            // fails, then we can't do much about it and the task bar icon won't show up again.
            HMODULE hLib = LoadLibrary(_T("user32.dll"));
            if (hLib)
            {
                m_pChangeWindowMessageFilter = reinterpret_cast<CTrayWindow::PFNCHANGEWINDOWMESSAGEFILTEREX>(GetProcAddress(hLib, "ChangeWindowMessageFilterEx"));
                if (m_pChangeWindowMessageFilter)
                {
                    (*m_pChangeWindowMessageFilter)(m_hwnd, WM_TASKBARCREATED, MSGFLT_ALLOW, nullptr);
                    (*m_pChangeWindowMessageFilter)(m_hwnd, WM_SETTINGCHANGE, MSGFLT_ALLOW, nullptr);
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
    SecureZeroMemory(&m_niData, sizeof(m_niData));

    ULONGLONG ullVersion = GetDllVersion(_T("Shell32.dll"));
    if (ullVersion >= MAKEDLLVERULL(6, 0, 0, 0))
        m_niData.cbSize = sizeof(NOTIFYICONDATA);
    else if (ullVersion >= MAKEDLLVERULL(5, 0, 0, 0))
        m_niData.cbSize = NOTIFYICONDATA_V2_SIZE;
    else
        m_niData.cbSize = NOTIFYICONDATA_V1_SIZE;

    m_niData.uID    = IDI_CryptSync;
    m_niData.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_INFO;

    m_niData.hIcon            = static_cast<HICON>(LoadImage(hResource, MAKEINTRESOURCE(IDI_CryptSync),
                                                  IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR));
    m_niData.hWnd             = *this;
    m_niData.uCallbackMessage = TRAY_WM_MESSAGE;
    m_niData.uVersion         = 6;

    Shell_NotifyIcon(NIM_DELETE, &m_niData);
    Shell_NotifyIcon(NIM_ADD, &m_niData);
    Shell_NotifyIcon(NIM_SETVERSION, &m_niData);
    DestroyIcon(m_niData.hIcon);
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
            m_watcher.ClearPaths();
            for (auto it = g_pairs.cbegin(); it != g_pairs.cend(); ++it)
            {
                std::wstring origPath  = it->origPath;
                std::wstring cryptPath = it->cryptPath;
                if ((it->syncDir == BothWays) || (it->syncDir == SrcToDst))
                    m_watcher.AddPath(origPath);
                if ((it->syncDir == BothWays) || (it->syncDir == DstToSrc))
                    m_watcher.AddPath(cryptPath);
            }
            SetTimer(*this, TIMER_DETECTCHANGES, TIMER_DETECTCHANGESINTERVAL, nullptr);
            SetTimer(*this, TIMER_FULLSCAN, TIMER_FULLSCANINTERVAL, nullptr);
            unsigned int threadId = 0;
            _beginthreadex(nullptr, 0, UpdateCheckThreadEntry, this, 0, &threadId);
            if (!m_bTrayMode)
                ::PostMessage(*this, WM_COMMAND, MAKEWPARAM(IDM_OPTIONS, 1), 0);
            m_folderSyncer.SetPairs(g_pairs);
            m_folderSyncer.SetTrayWnd(m_hwnd);
        }
        break;
        case WM_COMMAND:
            return DoCommand(LOWORD(wParam));
        case WM_PROGRESS:
        {
            m_itemsProcessed      = static_cast<int>(wParam);
            m_totalItemsToProcess = static_cast<int>(lParam);
        }
        break;
        case TRAY_WM_MESSAGE:
        {
            switch (lParam)
            {
                case WM_RBUTTONUP:
                case WM_CONTEXTMENU:
                {
                    POINT pt;
                    GetCursorPos(&pt);
                    HMENU hMenu    = LoadMenu(hResource, MAKEINTRESOURCE(IDC_CryptSync));
                    HMENU hPopMenu = GetSubMenu(hMenu, 0);
                    SetForegroundWindow(*this);
                    TrackPopupMenu(hPopMenu, TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, 0, *this, nullptr);
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
                    int   count    = static_cast<int>(m_folderSyncer.GetFailureCount());
                    WCHAR buf[200] = {0};
                    if (count)
                        swprintf_s(buf, L"%d items failed to synchronize", count);
                    else if (m_totalItemsToProcess)
                        swprintf_s(buf, L"Synched %d of %d items", m_itemsProcessed, m_totalItemsToProcess);
                    else
                        wcscpy_s(buf, L"synchronization ok");
                    m_niData.uFlags = NIF_TIP;
                    wcsncpy_s(m_niData.szTip, _countof(m_niData.szTip), buf, _countof(m_niData.szTip));
                    Shell_NotifyIcon(NIM_MODIFY, &m_niData);
                }
                break;
            }
        }
        break;
        case WM_DESTROY:
            Shell_NotifyIcon(NIM_DELETE, &m_niData);
            PostQuitMessage(0);
            break;
        case WM_TIMER:
            switch (wParam)
            {
                case TIMER_DETECTCHANGES:
                {
                    SetTimer(*this, TIMER_DETECTCHANGES, TIMER_DETECTCHANGESINTERVAL, nullptr);
                    if (!m_lastChangedPaths.empty())
                    {
                        for (auto it = m_lastChangedPaths.cbegin(); it != m_lastChangedPaths.cend(); ++it)
                        {
                            if (CIgnores::Instance().IsIgnored(*it))
                                continue;

                            m_folderSyncer.SyncFile(*it);
                        }
                    }
                    m_lastChangedPaths = m_watcher.GetChangedPaths();
                    auto ignores       = m_folderSyncer.GetNotifyIgnores();
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
                    size_t watchPathCount = 0;
                    for (auto it = g_pairs.cbegin(); it != g_pairs.cend(); ++it)
                    {
                        if ((it->syncDir == BothWays) || (it->syncDir == DstToSrc))
                            ++watchPathCount;
                        if ((it->syncDir == BothWays) || (it->syncDir == SrcToDst))
                            ++watchPathCount;
                    }

                    if (m_watcher.GetNumberOfWatchedPaths() != watchPathCount)
                    {
                        m_watcher.ClearPaths();
                        for (auto it = g_pairs.cbegin(); it != g_pairs.cend(); ++it)
                        {
                            std::wstring origPath  = it->origPath;
                            std::wstring cryptPath = it->cryptPath;
                            if ((it->syncDir == BothWays) || (it->syncDir == SrcToDst))
                                m_watcher.AddPath(origPath);
                            if ((it->syncDir == BothWays) || (it->syncDir == DstToSrc))
                                m_watcher.AddPath(cryptPath);
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

                                m_folderSyncer.SyncFile(*it);
                            }
                        }
                        m_lastChangedPaths = m_watcher.GetChangedPaths();
                        auto ignores       = m_folderSyncer.GetNotifyIgnores();
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
                    m_folderSyncer.SyncFolders(g_pairs);
                    m_watcher.ClearPaths();
                    for (auto it = g_pairs.cbegin(); it != g_pairs.cend(); ++it)
                    {
                        std::wstring origPath  = it->origPath;
                        std::wstring cryptPath = it->cryptPath;
                        if ((it->syncDir == BothWays) || (it->syncDir == SrcToDst))
                            m_watcher.AddPath(origPath);
                        if ((it->syncDir == BothWays) || (it->syncDir == DstToSrc))
                            m_watcher.AddPath(cryptPath);
                    }
                    SetTimer(*this, TIMER_FULLSCAN, TIMER_FULLSCANINTERVAL, nullptr);
                }
                break;
            }
            break;
        case WM_QUERYENDSESSION:
            m_folderSyncer.Stop();
            m_watcher.Stop();
            return TRUE;
        case WM_CLOSE:
        case WM_ENDSESSION:
        case WM_QUIT:
            m_folderSyncer.Stop();
            m_watcher.Stop();
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
            Shell_NotifyIcon(NIM_DELETE, &m_niData);
            m_watcher.Stop();
            ::PostQuitMessage(0);
            return 0;
        case IDM_ABOUT:
        {
            CAboutDlg dlg(nullptr);
            dlg.DoModal(hResource, IDD_ABOUTBOX, nullptr);
        }
        break;
        case IDM_OPTIONS:
        {
            if (m_bOptionsDialogShown)
                break;
            m_bOptionsDialogShown = true;
            COptionsDlg dlg(nullptr);
            dlg.SetUpdateAvailable(m_bNewerVersionAvailable);
            dlg.SetFailures(m_folderSyncer.GetFailures());
            INT_PTR ret           = dlg.DoModal(hResource, IDD_OPTIONS, nullptr);
            m_bOptionsDialogShown = false;
            if ((ret == IDOK) || (ret == IDCANCEL))
            {
                TIMER_FULLSCANINTERVAL = CRegStdDWORD(L"Software\\CryptSync\\FullScanInterval", 60000 * 30);
                m_folderSyncer.SyncFolders(g_pairs);
                m_watcher.ClearPaths();
                for (auto it = g_pairs.cbegin(); it != g_pairs.cend(); ++it)
                {
                    std::wstring origPath  = it->origPath;
                    std::wstring cryptPath = it->cryptPath;
                    if ((it->syncDir == BothWays) || (it->syncDir == SrcToDst))
                        m_watcher.AddPath(origPath);
                    if ((it->syncDir == BothWays) || (it->syncDir == DstToSrc))
                        m_watcher.AddPath(cryptPath);
                }
                SetTimer(*this, TIMER_DETECTCHANGES, TIMER_DETECTCHANGESINTERVAL, nullptr);
                SetTimer(*this, TIMER_FULLSCAN, TIMER_FULLSCANINTERVAL, nullptr);
            }
            else
            {
                Shell_NotifyIcon(NIM_DELETE, &m_niData);
                m_watcher.Stop();
                ::PostQuitMessage(0);
                return 0;
            }
        }
        break;
        case ID_FILE_SYNCNOW:
            SetTimer(*this, TIMER_FULLSCAN, 1, nullptr);
            break;
        default:
            break;
    };
    return 1;
}

unsigned int CTrayWindow::UpdateCheckThreadEntry(void* pContext)
{
    static_cast<CTrayWindow*>(pContext)->UpdateCheckThread();
    _endthreadex(0);
    return 0;
}

void CTrayWindow::UpdateCheckThread()
{
    m_bNewerVersionAvailable = CUpdateDlg::CheckNewer();
    if (m_bNewerVersionAvailable && m_bTrayMode)
    {
        CUpdateDlg dlg(nullptr);
        dlg.DoModal(hResource, IDD_NEWERNOTIFYDLG, nullptr);
        m_bNewerVersionAvailable = false;
    }
}
