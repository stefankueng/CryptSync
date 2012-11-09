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

#include "stdafx.h"
#include "resource.h"
#include "OptionsDlg.h"
#include "Registry.h"
#include "PairAddDlg.h"
#include "UpdateDlg.h"
#include "AboutDlg.h"
#include "TextDlg.h"

#include <string>
#include <algorithm>
#include <Commdlg.h>

COptionsDlg::COptionsDlg(HWND hParent)
    : m_hParent(hParent)
    , m_bNewerVersionAvailable(false)
{
}

COptionsDlg::~COptionsDlg(void)
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

            AddToolTip(IDC_AUTOSTART, _T("Starts CryptSync automatically when Windows starts up."));

            // initialize the controls
            bool bStartWithWindows = !std::wstring(CRegStdString(_T("Software\\Microsoft\\Windows\\CurrentVersion\\Run\\CryptSync"))).empty();
            SendDlgItemMessage(*this, IDC_AUTOSTART, BM_SETCHECK, bStartWithWindows ? BST_CHECKED : BST_UNCHECKED, NULL);
            std::wstring sIgnores = CRegStdString(L"Software\\CryptSync\\Ignores", L"*.tmp*|~*.*");
            SetDlgItemText(*this, IDC_IGNORE, sIgnores.c_str());

            InitPairList();

            DialogEnableWindow(IDC_SHOWFAILURES, m_failures.size()>0);

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
        EndDialog(*this, IDEXIT);
        return TRUE;
    case WM_NOTIFY:
        {
            if (wParam == IDC_SYNCPAIRS)
            {
                DoListNotify((LPNMITEMACTIVATE)lParam);
            }
        }
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
            CRegStdString regStartWithWindows = CRegStdString(_T("Software\\Microsoft\\Windows\\CurrentVersion\\Run\\CryptSync"));
            bool bStartWithWindows = !!SendDlgItemMessage(*this, IDC_AUTOSTART, BM_GETCHECK, 0, NULL);
            if (bStartWithWindows)
            {
                TCHAR buf[MAX_PATH*4];
                GetModuleFileName(NULL, buf, MAX_PATH*4);
                std::wstring cmd = L"\"";
                cmd += std::wstring(buf);
                cmd += L"\" /tray";
                regStartWithWindows = cmd;
            }
            else
                regStartWithWindows.removeValue();

            CRegStdString regIgnores = CRegStdString(L"Software\\CryptSync\\Ignores", L"*.tmp*|~*.*");
            auto ignoreText = GetDlgItemText(IDC_IGNORE);
            regIgnores = ignoreText.get();

            g_pairs.SavePairs();
        }
        // fall through
    case IDCANCEL:
    case IDEXIT:
        EndDialog(*this, id);
        break;
    case IDC_SYNCEXIT:
        {
            m_foldersync.SyncFolders(g_pairs, *this);
        }
        break;
    case IDC_CREATEPAIR:
        {
            CPairAddDlg dlg(*this);
            if (dlg.DoModal(hResource, IDD_PAIRADD, *this)==IDOK)
            {
                if (!dlg.m_origpath.empty() && !dlg.m_cryptpath.empty())
                {
                    if (g_pairs.AddPair(dlg.m_origpath, dlg.m_cryptpath, dlg.m_password, dlg.m_encnames, dlg.m_oneway))
                        InitPairList();
                    g_pairs.SavePairs();
                }
            }
        }
        break;
    case IDC_DELETEPAIR:
        {
            HWND hListControl = GetDlgItem(*this, IDC_SYNCPAIRS);
            int nCount = ListView_GetItemCount(hListControl);
            if (nCount == 0)
                break;
            int iItem = -1;
            PairVector sels;
            while ((iItem = ListView_GetNextItem(hListControl, iItem, LVNI_SELECTED)) != (-1))
            {
                if ((iItem < 0)||(iItem >= (int)g_pairs.size()))
                    continue;
                sels.push_back(g_pairs[iItem]);
            }

            for (auto it = sels.cbegin(); it != sels.cend(); ++it)
            {
                auto foundit = std::find(g_pairs.cbegin(), g_pairs.cend(), *it);
                if (foundit != g_pairs.end())
                {
                    g_pairs.erase(foundit);
                }
            }
            InitPairList();
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
    }
    return 1;
}

void COptionsDlg::InitPairList()
{
    HWND hListControl = GetDlgItem(*this, IDC_SYNCPAIRS);
    DWORD exStyle = LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT;
    ListView_DeleteAllItems(hListControl);

    int c = Header_GetItemCount(ListView_GetHeader(hListControl))-1;
    while (c>=0)
        ListView_DeleteColumn(hListControl, c--);

    ListView_SetExtendedListViewStyle(hListControl, exStyle);
    LVCOLUMN lvc = {0};
    lvc.mask = LVCF_TEXT;
    lvc.fmt = LVCFMT_LEFT;
    lvc.cx = -1;
    lvc.pszText = _T("Original");
    ListView_InsertColumn(hListControl, 0, &lvc);
    lvc.pszText = _T("Encrypted");
    ListView_InsertColumn(hListControl, 1, &lvc);
    lvc.pszText = _T("Sync failures");
    ListView_InsertColumn(hListControl, 2, &lvc);


    for (auto it = g_pairs.cbegin(); it != g_pairs.cend(); ++it)
    {
        std::wstring origpath = std::get<0>(*it);
        std::wstring cryptpath = std::get<1>(*it);
        LVITEM lv = {0};
        lv.mask = LVIF_TEXT;
        std::unique_ptr<WCHAR[]> varbuf(new WCHAR[origpath.size()+1]);
        _tcscpy_s(varbuf.get(), origpath.size()+1, origpath.c_str());
        lv.pszText = varbuf.get();
        lv.iItem = ListView_GetItemCount(hListControl);
        int ret = ListView_InsertItem(hListControl, &lv);
        if (ret >= 0)
        {
            lv.iItem = ret;
            lv.iSubItem = 1;
            varbuf = std::unique_ptr<WCHAR[]>(new TCHAR[cryptpath.size()+1]);
            lv.pszText = varbuf.get();
            _tcscpy_s(lv.pszText, cryptpath.size()+1, cryptpath.c_str());
            ListView_SetItem(hListControl, &lv);

            lv.iSubItem = 2;
            WCHAR buf[10] = {0};
            lv.pszText = buf;
            int failures = GetFailuresFor(origpath);
            if (failures)
                swprintf_s(buf, L"%d", failures);
            else
                wcscpy_s(buf, L"none");
            ListView_SetItem(hListControl, &lv);
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
        if (lpNMItemActivate->iItem >= 0)
        {
            auto t = g_pairs[lpNMItemActivate->iItem];

            CPairAddDlg dlg(*this);
            dlg.m_origpath = std::get<0>(t);
            dlg.m_cryptpath = std::get<1>(t);
            dlg.m_password = std::get<2>(t);
            dlg.m_encnames = std::get<3>(t);
            dlg.m_oneway = std::get<4>(t);
            if (dlg.DoModal(hResource, IDD_PAIRADD, *this)==IDOK)
            {
                if (!dlg.m_origpath.empty() && !dlg.m_cryptpath.empty())
                {
                    g_pairs.erase(g_pairs.begin()+lpNMItemActivate->iItem);
                    if (g_pairs.AddPair(dlg.m_origpath, dlg.m_cryptpath, dlg.m_password, dlg.m_encnames, dlg.m_oneway))
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
    }
}

int COptionsDlg::GetFailuresFor( const std::wstring& path )
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
