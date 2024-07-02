// CryptSync - A folder sync tool with encryption

// Copyright (C) 2012-2014, 2016, 2019, 2021 - Stefan Kueng

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
#include "resource.h"
#include "OptionsDlg.h"
#include "Registry.h"
#include "PairAddDlg.h"
#include "UpdateDlg.h"
#include "AboutDlg.h"
#include "TextDlg.h"
#include "Ignores.h"
#include "StringUtils.h"
#include "CircularLog.h"
#include "ResString.h"
#include "OnOutOfScope.h"

#include <string>
#include <algorithm>
#include <Commdlg.h>

COptionsDlg::COptionsDlg(HWND hParent)
    : m_hParent(hParent)
    , m_bNewerVersionAvailable(false)
    , m_exitAfterSync(false)
    , m_listInit(false)
{
}

COptionsDlg::~COptionsDlg()
{
}

LRESULT COptionsDlg::DlgFunc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (uMsg)
    {
        case WM_INITDIALOG:
        {
            InitDialog(hwndDlg, IDI_CryptSync);

            m_link.ConvertStaticToHyperlink(hwndDlg, IDC_ABOUT, _T(""));

            AddToolTip(IDC_AUTOSTART, L"Starts CryptSync automatically when Windows starts up.");
            AddToolTip(IDC_IGNORELABEL, L"Ignore masks, separated by '|' example: *.tmp|~*.*");
            AddToolTip(IDC_IGNORE, L"Ignore masks, separated by '|' example: *.tmp|~*.*");

            // initialize the controls
            CRegStdString regStart(_T("Software\\Microsoft\\Windows\\CurrentVersion\\Run\\CryptSync"));
            bool          bStartWithWindows = !std::wstring(regStart).empty();
            SendDlgItemMessage(*this, IDC_AUTOSTART, BM_SETCHECK, bStartWithWindows ? BST_CHECKED : BST_UNCHECKED, NULL);
            SendDlgItemMessage(*this, IDC_AUTOSTART, BM_SETCHECK, bStartWithWindows ? BST_CHECKED : BST_UNCHECKED, NULL);
            CRegStdString regIgnores(L"Software\\CryptSync\\Ignores", DEFAULT_IGNORES);
            std::wstring  sIgnores = regIgnores;
            SetDlgItemText(*this, IDC_IGNORE, sIgnores.c_str());

            CRegStdDWORD regInterval(L"Software\\CryptSync\\FullScanInterval", 60000 * 30);
            UINT         intVal = static_cast<DWORD>(regInterval);
            intVal /= 60000;
            std::wstring sInterval = CStringUtils::Format(L"%d", intVal);
            SetDlgItemText(*this, IDC_INTERVAL, sInterval.c_str());

            InitPairList();

            DialogEnableWindow(IDC_SHOWFAILURES, !m_failures.empty());

            if (m_bNewerVersionAvailable)
            {
                CUpdateDlg dlg(*this);
                dlg.DoModal(hResource, IDD_NEWERNOTIFYDLG, *this);
                m_bNewerVersionAvailable = false;
            }
        }
            return TRUE;
        case WM_COMMAND:
            return DoCommand(LOWORD(wParam));
        case WM_THREADENDED:
            if (m_exitAfterSync)
            {
                EndDialog(*this, IDEXIT);
                return TRUE;
            }
            return FALSE;
        case WM_NOTIFY:
        {
            if (wParam == IDC_SYNCPAIRS)
            {
                DoListNotify(reinterpret_cast<LPNMITEMACTIVATE>(lParam));
            }
        }
            return FALSE;
        case WM_CONTEXTMENU:
        {
            POINT pt;
            GetCursorPos(&pt);
            HMENU hMenu    = LoadMenu(hResource, MAKEINTRESOURCE(IDR_PAIRMENU));
            HMENU hPopMenu = GetSubMenu(hMenu, 0);
            TrackPopupMenu(hPopMenu, TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, 0, *this, nullptr);
            DestroyMenu(hMenu);
        }
            return FALSE;
        default:
            return FALSE;
    }
}

LRESULT COptionsDlg::DoCommand(int id)
{
    switch (id)
    {
        case IDOK:
        {
            SaveSettings();
        }
            [[fallthrough]];
        case IDCANCEL:
            [[fallthrough]];
        case IDEXIT:
            if ((id == IDEXIT) && MessageBox(*this, L"Are you sure you want to quit?", L"CryptSync", MB_ICONQUESTION | MB_YESNO) != IDYES)
                return 1;
            EndDialog(*this, id);
            break;
        case IDC_SHOWLOG:
        {
            CCircularLog::Instance().Save();
            std::wstring     path = CCircularLog::Instance().GetSavePath();
            SHELLEXECUTEINFO shex = {0};
            shex.cbSize           = sizeof(SHELLEXECUTEINFO);
            shex.fMask            = SEE_MASK_DOENVSUBST | SEE_MASK_ASYNCOK | SEE_MASK_CLASSNAME;
            shex.hwnd             = *this;
            shex.lpVerb           = nullptr;
            shex.lpFile           = path.c_str();
            shex.lpClass          = L".txt";
            shex.nShow            = SW_SHOWNORMAL;
            if (!ShellExecuteEx(&shex))
            {
                shex.fMask        = SEE_MASK_DOENVSUBST | SEE_MASK_ASYNCOK;
                shex.hwnd         = *this;
                shex.lpFile       = L"%windir%\\notepad.exe";
                shex.lpParameters = path.c_str();
                shex.nShow        = SW_SHOWNORMAL;
                ShellExecuteEx(&shex);
            }
        }
        break;
        case IDC_SYNCEXIT:
        case ID_FILE_SYNCNOW:
        {
            SaveSettings();
            m_exitAfterSync = id == IDC_SYNCEXIT;
            m_folderSync.SyncFolders(g_pairs, *this);
        }
        break;
        case IDC_CREATEPAIR:
        {
            CPairAddDlg dlg(*this);
            if (dlg.DoModal(hResource, IDD_PAIRADD, *this) == IDOK)
            {
                if (!dlg.m_origPath.empty() && !dlg.m_cryptPath.empty())
                {
                    if (g_pairs.AddPair(true, dlg.m_origPath, dlg.m_cryptPath, dlg.m_password, dlg.m_cryptOnly, dlg.m_copyOnly, dlg.m_noSync, dlg.m_compressSize, dlg.m_encNames, dlg.m_encNamesNew, dlg.m_syncDir, dlg.m_7ZExt, dlg.m_useGpg, dlg.m_fat, dlg.m_syncDeleted, dlg.m_ResetOriginalArchAttr))
                        InitPairList();
                    g_pairs.SavePairs();
                }
            }
        }
        break;
        case ID_EDIT:
        case IDC_EDITPAIR:
        {
            HWND hListControl = GetDlgItem(*this, IDC_SYNCPAIRS);
            int  nCount       = ListView_GetItemCount(hListControl);
            if (nCount == 0)
                break;
            int iItem = -1;
            while ((iItem = ListView_GetNextItem(hListControl, iItem, LVNI_SELECTED)) != (-1))
            {
                if ((iItem < 0) || (iItem >= static_cast<int>(g_pairs.size())))
                    continue;
                auto        t = g_pairs[iItem];
                CPairAddDlg dlg(*this);
                dlg.m_origPath     = t.m_origPath;
                dlg.m_cryptPath    = t.m_cryptPath;
                dlg.m_password     = t.m_password;
                dlg.m_cryptOnly    = t.cryptOnly();
                dlg.m_copyOnly     = t.copyOnly();
                dlg.m_noSync       = t.noSync();
                dlg.m_encNames     = t.m_encNames;
                dlg.m_encNamesNew  = t.m_encNamesNew;
                dlg.m_syncDir      = t.m_syncDir;
                dlg.m_7ZExt        = t.m_use7Z;
                dlg.m_useGpg       = t.m_useGpg;
                dlg.m_fat          = t.m_fat;
                dlg.m_compressSize = t.m_compressSize;
                dlg.m_syncDeleted  = t.m_syncDeleted;
                dlg.m_ResetOriginalArchAttr = t.m_ResetOriginalArchAttr;
                if (dlg.DoModal(hResource, IDD_PAIRADD, *this) == IDOK)
                {
                    if (!dlg.m_origPath.empty() && !dlg.m_cryptPath.empty())
                    {
                        g_pairs.erase(g_pairs.begin() + iItem);
                        if (g_pairs.AddPair(true, dlg.m_origPath, dlg.m_cryptPath, dlg.m_password, dlg.m_cryptOnly, dlg.m_copyOnly, dlg.m_noSync, dlg.m_compressSize, dlg.m_encNames, dlg.m_encNamesNew, dlg.m_syncDir, dlg.m_7ZExt, dlg.m_useGpg, dlg.m_fat, dlg.m_syncDeleted, dlg.m_ResetOriginalArchAttr))
                            InitPairList();
                        g_pairs.SavePairs();
                    }
                }
                break;
            }
        }
        break;
        case ID_DELETE:
        case IDC_DELETEPAIR:
        {
            HWND hListControl = GetDlgItem(*this, IDC_SYNCPAIRS);
            int  nCount       = ListView_GetItemCount(hListControl);
            if (nCount == 0)
                break;

            int        iItem = -1;
            PairVector sels;
            while ((iItem = ListView_GetNextItem(hListControl, iItem, LVNI_SELECTED)) != (-1))
            {
                if ((iItem < 0) || (iItem >= static_cast<int>(g_pairs.size())))
                    continue;
                sels.push_back(g_pairs[iItem]);
            }

            if (!sels.empty())
            {
                ResString rDelquestion(hResource, IDS_ASK_DELETEPAIR);
                auto      sQuestion = CStringUtils::Format(rDelquestion, static_cast<int>(sels.size()));
                if (MessageBox(*this, sQuestion.c_str(), L"CryptSync", MB_YESNO | MB_DEFBUTTON2) != IDYES)
                    break;
            }

            for (auto it = sels.cbegin(); it != sels.cend(); ++it)
            {
                auto foundIt = std::find(g_pairs.cbegin(), g_pairs.cend(), *it);
                if (foundIt != g_pairs.end())
                {
                    g_pairs.erase(foundIt);
                }
            }
            InitPairList();
            g_pairs.SavePairs();
        }
        break;
        case IDC_ABOUT:
        {
            CAboutDlg dlgAbout(*this);
            dlgAbout.DoModal(hResource, IDD_ABOUTBOX, *this);
        }
        break;
        case IDC_SHOWFAILURES:
        {
            std::wstring sFailures = L"the following paths failed to sync:\r\n";
            for (auto it = m_failures.cbegin(); it != m_failures.cend(); ++it)
            {
                if (it->second == Encrypt)
                    sFailures += L"Encrypting : ";
                else
                    sFailures += L"Decrypting : ";
                sFailures += it->first;
                sFailures += L"\r\n";
            }
            CTextDlg dlg(*this);
            dlg.m_text = sFailures;
            dlg.DoModal(hResource, IDD_TEXTDLG, *this);
        }
        break;
        case ID_SYNCNOW:
        case ID_SYNCNOWANDEXIT:
        {
            SaveSettings();
            HWND hListControl = GetDlgItem(*this, IDC_SYNCPAIRS);
            int  nCount       = ListView_GetItemCount(hListControl);
            if (nCount == 0)
                break;
            int        iItem = -1;
            PairVector sels;
            while ((iItem = ListView_GetNextItem(hListControl, iItem, LVNI_SELECTED)) != (-1))
            {
                if ((iItem < 0) || (iItem >= static_cast<int>(g_pairs.size())))
                    continue;
                sels.push_back(g_pairs[iItem]);
            }
            m_exitAfterSync = (id == ID_SYNCNOWANDEXIT);
            m_folderSync.SyncFolders(sels, *this);
        }
        break;
    }
    return 1;
}

void COptionsDlg::InitPairList()
{
    m_listInit = true;
    OnOutOfScope(m_listInit = false);

    HWND  hListControl = GetDlgItem(*this, IDC_SYNCPAIRS);
    DWORD exStyle      = LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT | LVS_EX_CHECKBOXES;
    ListView_DeleteAllItems(hListControl);

    int c = Header_GetItemCount(ListView_GetHeader(hListControl)) - 1;
    while (c >= 0)
        ListView_DeleteColumn(hListControl, c--);

    ListView_SetExtendedListViewStyle(hListControl, exStyle);
    LVCOLUMN lvc = {0};
    lvc.mask     = LVCF_TEXT;
    lvc.fmt      = LVCFMT_LEFT;
    lvc.cx       = -1;
    lvc.pszText  = _T("Original");
    ListView_InsertColumn(hListControl, 0, &lvc);
    lvc.pszText = _T("Encrypted");
    ListView_InsertColumn(hListControl, 1, &lvc);
    lvc.pszText = _T("Sync failures");
    ListView_InsertColumn(hListControl, 2, &lvc);

    for (auto it = g_pairs.cbegin(); it != g_pairs.cend(); ++it)
    {
        std::wstring origPath  = it->m_origPath;
        std::wstring cryptPath = it->m_cryptPath;
        LVITEM       lv        = {0};
        lv.mask                = LVIF_TEXT;
        auto varBuf            = std::make_unique<WCHAR[]>(origPath.size() + 1);
        _tcscpy_s(varBuf.get(), origPath.size() + 1, origPath.c_str());
        lv.pszText = varBuf.get();
        lv.iItem   = ListView_GetItemCount(hListControl);
        int ret    = ListView_InsertItem(hListControl, &lv);
        if (ret >= 0)
        {
            lv.iItem    = ret;
            lv.iSubItem = 1;
            varBuf      = std::make_unique<WCHAR[]>(cryptPath.size() + 1);
            lv.pszText  = varBuf.get();
            _tcscpy_s(lv.pszText, cryptPath.size() + 1, cryptPath.c_str());
            ListView_SetItem(hListControl, &lv);

            lv.iSubItem   = 2;
            WCHAR buf[10] = {0};
            lv.pszText    = buf;
            int failures  = GetFailuresFor(origPath);
            if (failures)
                swprintf_s(buf, L"%d", failures);
            else
                wcscpy_s(buf, L"none");
            ListView_SetItem(hListControl, &lv);
            ListView_SetCheckState(hListControl, ret, it->m_enabled);
        }
    }

    ListView_SetColumnWidth(hListControl, 0, LVSCW_AUTOSIZE_USEHEADER);
    ListView_SetColumnWidth(hListControl, 1, LVSCW_AUTOSIZE_USEHEADER);
    ListView_SetColumnWidth(hListControl, 2, LVSCW_AUTOSIZE_USEHEADER);
}

void COptionsDlg::DoListNotify(LPNMITEMACTIVATE lpNMItemActivate)
{
    if (lpNMItemActivate->hdr.code == NM_DBLCLK)
    {
        if ((lpNMItemActivate->iItem >= 0) && (lpNMItemActivate->iItem < static_cast<int>(g_pairs.size())))
        {
            auto t = g_pairs[lpNMItemActivate->iItem];

            CPairAddDlg dlg(*this);
            dlg.m_origPath     = t.m_origPath;
            dlg.m_cryptPath    = t.m_cryptPath;
            dlg.m_password     = t.m_password;
            dlg.m_cryptOnly    = t.cryptOnly();
            dlg.m_copyOnly     = t.copyOnly();
            dlg.m_noSync       = t.noSync();
            dlg.m_encNames     = t.m_encNames;
            dlg.m_encNamesNew  = t.m_encNamesNew;
            dlg.m_syncDir      = t.m_syncDir;
            dlg.m_7ZExt        = t.m_use7Z;
            dlg.m_useGpg       = t.m_useGpg;
            dlg.m_fat          = t.m_fat;
            dlg.m_compressSize = t.m_compressSize;
            dlg.m_syncDeleted  = t.m_syncDeleted;
            dlg.m_ResetOriginalArchAttr = t.m_ResetOriginalArchAttr;
            if (dlg.DoModal(hResource, IDD_PAIRADD, *this) == IDOK)
            {
                if (!dlg.m_origPath.empty() && !dlg.m_cryptPath.empty())
                {
                    g_pairs.erase(g_pairs.begin() + lpNMItemActivate->iItem);
                    if (g_pairs.AddPair(true, dlg.m_origPath, dlg.m_cryptPath, dlg.m_password, dlg.m_cryptOnly, dlg.m_copyOnly, dlg.m_noSync, dlg.m_compressSize, dlg.m_encNames, dlg.m_encNamesNew, dlg.m_syncDir, dlg.m_7ZExt, dlg.m_useGpg, dlg.m_fat, dlg.m_syncDeleted, dlg.m_ResetOriginalArchAttr))
                        InitPairList();
                    g_pairs.SavePairs();
                }
            }
        }
    }
    else if (lpNMItemActivate->hdr.code == LVN_ITEMCHANGED)
    {
        HWND hListControl = GetDlgItem(*this, IDC_SYNCPAIRS);
        DialogEnableWindow(IDC_DELETEPAIR, (ListView_GetSelectedCount(hListControl)) > 0);
        DialogEnableWindow(IDC_EDITPAIR, (ListView_GetSelectedCount(hListControl)) == 1);
        if (!m_listInit && (lpNMItemActivate->uNewState & LVIS_STATEIMAGEMASK) != 0 && (lpNMItemActivate->iItem >= 0) && (lpNMItemActivate->iItem < static_cast<int>(g_pairs.size())))
        {
            auto& t     = g_pairs[lpNMItemActivate->iItem];
            t.m_enabled = ListView_GetCheckState(hListControl, lpNMItemActivate->iItem);
            g_pairs.SavePairs();
        }
    }
}

int COptionsDlg::GetFailuresFor(const std::wstring& path) const
{
    int failures = 0;

    for (auto it = m_failures.cbegin(); it != m_failures.cend(); ++it)
    {
        if (it->first.size() > path.size())
        {
            if (it->first.substr(0, path.size()) == path)
            {
                if (it->first[path.size()] == '\\')
                    failures++;
            }
        }
    }
    return failures;
}

void COptionsDlg::SaveSettings()
{
    CRegStdString regStartWithWindows = CRegStdString(_T("Software\\Microsoft\\Windows\\CurrentVersion\\Run\\CryptSync"));
    bool          bStartWithWindows   = !!SendDlgItemMessage(*this, IDC_AUTOSTART, BM_GETCHECK, 0, NULL);
    if (bStartWithWindows)
    {
        TCHAR buf[MAX_PATH * 4];
        GetModuleFileName(nullptr, buf, _countof(buf));
        std::wstring cmd = L"\"";
        cmd += std::wstring(buf);
        cmd += L"\" /tray";
        regStartWithWindows = cmd;
    }
    else
        regStartWithWindows.removeValue();

    // ReSharper disable once CppEntityAssignedButNoRead
    CRegStdString regIgnores(L"Software\\CryptSync\\Ignores", DEFAULT_IGNORES);
    auto          ignoreText = GetDlgItemText(IDC_IGNORE);
    regIgnores               = ignoreText.get();

    // ReSharper disable once CppEntityAssignedButNoRead
    CRegStdDWORD regInterval(L"Software\\CryptSync\\FullScanInterval", 60000 * 30);
    auto         intervalText = GetDlgItemText(IDC_INTERVAL);
    DWORD        intVal       = _wtoi(intervalText.get());
    if (intVal > 0)
        regInterval = intVal * 60000;

    HWND hListControl = GetDlgItem(*this, IDC_SYNCPAIRS);
    int  listIndex    = 0;
    for (auto& pair : g_pairs)
    {
        pair.m_enabled = ListView_GetCheckState(hListControl, listIndex);
        ++listIndex;
    }

    g_pairs.SavePairs();

    CIgnores::Instance().Reload();
}
