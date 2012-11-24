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
#include "PairAddDlg.h"
#include "BrowseFolder.h"
#include <string>
#include <Commdlg.h>
#include <cctype>
#include <algorithm>

CPairAddDlg::CPairAddDlg(HWND hParent)
    : m_hParent(hParent)
    , m_encnames(false)
    , m_oneway(false)
    , m_pDropTargetOrig(nullptr)
    , m_pDropTargetCrypt(nullptr)
{
}

CPairAddDlg::~CPairAddDlg(void)
{
    delete m_pDropTargetOrig;
    delete m_pDropTargetCrypt;
}

LRESULT CPairAddDlg::DlgFunc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
        {
            InitDialog(hwndDlg, IDI_CryptSync);

            // initialize the controls
            SetDlgItemText(hwndDlg, IDC_ORIGPATH, m_origpath.c_str());
            SetDlgItemText(hwndDlg, IDC_CRYPTPATH, m_cryptpath.c_str());
            SetDlgItemText(hwndDlg, IDC_PASSWORD, m_password.c_str());
            SetDlgItemText(hwndDlg, IDC_PASSWORD2, m_password.c_str());
            SendDlgItemMessage(*this, IDC_ENCNAMES, BM_SETCHECK, m_encnames ? BST_CHECKED : BST_UNCHECKED, NULL);
            SendDlgItemMessage(*this, IDC_ONEWAY, BM_SETCHECK, m_oneway ? BST_CHECKED : BST_UNCHECKED, NULL);

            AddToolTip(IDC_ONEWAY, L"if this is checked, changes in the encrypted folder are not synchronized back to the original folder!");

            // the path edit control should work as a drop target for files and folders
            HWND hOrigPath = GetDlgItem(hwndDlg, IDC_ORIGPATH);
            m_pDropTargetOrig = new CFileDropTarget(hOrigPath);
            RegisterDragDrop(hOrigPath, m_pDropTargetOrig);
            FORMATETC ftetc={0};
            ftetc.cfFormat = CF_TEXT;
            ftetc.dwAspect = DVASPECT_CONTENT;
            ftetc.lindex = -1;
            ftetc.tymed = TYMED_HGLOBAL;
            m_pDropTargetOrig->AddSuportedFormat(ftetc);
            ftetc.cfFormat=CF_HDROP;
            m_pDropTargetOrig->AddSuportedFormat(ftetc);
            SHAutoComplete(GetDlgItem(*this, IDC_ORIGPATH), SHACF_FILESYSTEM|SHACF_AUTOSUGGEST_FORCE_ON);

            HWND hCryptPath = GetDlgItem(hwndDlg, IDC_CRYPTPATH);
            m_pDropTargetCrypt = new CFileDropTarget(hCryptPath);
            RegisterDragDrop(hCryptPath, m_pDropTargetCrypt);
            ftetc.cfFormat = CF_TEXT;
            ftetc.dwAspect = DVASPECT_CONTENT;
            ftetc.lindex = -1;
            ftetc.tymed = TYMED_HGLOBAL;
            m_pDropTargetCrypt->AddSuportedFormat(ftetc);
            ftetc.cfFormat = CF_HDROP;
            m_pDropTargetCrypt->AddSuportedFormat(ftetc);
            SHAutoComplete(GetDlgItem(*this, IDC_CRYPTPATH), SHACF_FILESYSTEM|SHACF_AUTOSUGGEST_FORCE_ON);

        }
        return TRUE;
    case WM_COMMAND:
        return DoCommand(LOWORD(wParam));
    default:
        return FALSE;
    }
}

LRESULT CPairAddDlg::DoCommand(int id)
{
    switch (id)
    {
    case IDOK:
        {
            auto buf = GetDlgItemText(IDC_ORIGPATH);
            m_origpath = buf.get();
            m_origpath.erase(m_origpath.find_last_not_of(L" \n\r\t\\")+1);

            buf = GetDlgItemText(IDC_CRYPTPATH);
            m_cryptpath = buf.get();
            m_cryptpath.erase(m_cryptpath.find_last_not_of(L" \n\r\t\\")+1);

            buf = GetDlgItemText(IDC_PASSWORD);
            m_password = buf.get();
            buf = GetDlgItemText(IDC_PASSWORD2);
            std::wstring retype = buf.get();
            if (m_password != retype)
            {
                ::MessageBox(*this, L"password do not match!\nPlease reenter the password.", L"Password mismatch", MB_ICONERROR);
                return 0;
            }
            if ( (m_password.find_first_of(L"\"") != std::string::npos) ||
                 (m_password.find_first_of(L"\'") != std::string::npos) ||
                 (m_password.find_first_of(L"\\") != std::string::npos) ||
                 (m_password.find_first_of(L"/") != std::string::npos) )
            {
                ::MessageBox(*this, L"password must not contain the following chars:\n\" \' / \\", L"Illegal Password char", MB_ICONERROR);
                return 0;
            }

            std::wstring origpath = m_origpath;
            std::wstring cryptpath = m_cryptpath;
            std::transform(origpath.begin(), origpath.end(), origpath.begin(), std::tolower);
            std::transform(cryptpath.begin(), cryptpath.end(), cryptpath.begin(), std::tolower);

            if (origpath == cryptpath)
            {
                ::MessageBox(*this, L"source and destination are identical!\nPlease choose different folders.", L"Paths invalid", MB_ICONERROR);
                return 0;
            }

            if ( ((origpath.size() > cryptpath.size()) && (origpath.substr(0, cryptpath.size()) == cryptpath) && (origpath[cryptpath.size()] == '\\')) ||
                 ((cryptpath.size() > origpath.size()) && (cryptpath.substr(0, origpath.size()) == origpath) && (cryptpath[origpath.size()] == '\\')))
            {
                ::MessageBox(*this, L"source and destination point to the same tree\nmake sure that one folder is not part of the other.", L"Paths invalid", MB_ICONERROR);
                return 0;
            }

            m_encnames = !!SendDlgItemMessage(*this, IDC_ENCNAMES, BM_GETCHECK, 0, NULL);
            m_oneway = !!SendDlgItemMessage(*this, IDC_ONEWAY, BM_GETCHECK, 0, NULL);
        }
        // fall through
    case IDCANCEL:
        EndDialog(*this, id);
        break;
    case IDC_BROWSEORIG:
        {
            CBrowseFolder browse;

            auto path = GetDlgItemText(IDC_ORIGPATH);
            std::unique_ptr<WCHAR[]> pathbuf(new WCHAR[MAX_PATH_NEW]);
            wcscpy_s(pathbuf.get(), MAX_PATH_NEW, path.get());
            browse.SetInfo(_T("Select path to search"));
            if (browse.Show(*this, pathbuf.get(), MAX_PATH_NEW, m_origpath.c_str()) == CBrowseFolder::OK)
            {
                SetDlgItemText(*this, IDC_ORIGPATH, pathbuf.get());
                m_origpath = pathbuf.get();
            }
        }
        break;
    case IDC_BROWSECRYPT:
        {
            CBrowseFolder browse;

            auto path = GetDlgItemText(IDC_CRYPTPATH);
            std::unique_ptr<WCHAR[]> pathbuf(new WCHAR[MAX_PATH_NEW]);
            wcscpy_s(pathbuf.get(), MAX_PATH_NEW, path.get());
            browse.SetInfo(_T("Select path to search"));
            if (browse.Show(*this, pathbuf.get(), MAX_PATH_NEW, m_cryptpath.c_str()) == CBrowseFolder::OK)
            {
                SetDlgItemText(*this, IDC_CRYPTPATH, pathbuf.get());
                m_cryptpath = pathbuf.get();
            }
        }
        break;
    }
    return 1;
}

