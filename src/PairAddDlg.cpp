// CryptSync - A folder sync tool with encryption

// Copyright (C) 2012-2016, 2019, 2021 - Stefan Kueng

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
#include <shlwapi.h>

CPairAddDlg::CPairAddDlg(HWND hParent)
    : m_compressSize(100)
    , m_encNames(false)
    , m_encNamesNew(false)
    , m_syncDir(BothWays)
    , m_7ZExt(true)
    , m_useGpg(false)
    , m_fat(true)
    , m_syncDeleted(true)
    , m_ResetOriginalArchAttr(false)
    , m_hParent(hParent)
    , m_pDropTargetOrig(nullptr)
    , m_pDropTargetCrypt(nullptr)
{
}

CPairAddDlg::~CPairAddDlg()
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
            SetDlgItemText(hwndDlg, IDC_ORIGPATH, m_origPath.c_str());
            SetDlgItemText(hwndDlg, IDC_CRYPTPATH, m_cryptPath.c_str());
            SetDlgItemText(hwndDlg, IDC_PASSWORD, m_password.c_str());
            SetDlgItemText(hwndDlg, IDC_PASSWORD2, m_password.c_str());
            SetDlgItemText(hwndDlg, IDC_NOCOMPRESS, m_cryptOnly.c_str());
            SetDlgItemText(hwndDlg, IDC_NOCRYPT, m_copyOnly.c_str());
            SetDlgItemText(hwndDlg, IDC_NOSYNC, m_noSync.c_str());
            SetDlgItemText(hwndDlg, IDC_COMPRESSSIZE, std::to_wstring(m_compressSize).c_str());
            SendDlgItemMessage(*this, IDC_ENCNAMES, BM_SETCHECK, m_encNames ? BST_CHECKED : BST_UNCHECKED, NULL);
            SendDlgItemMessage(*this, IDC_NEWNAMEENCRYPTION, BM_SETCHECK, m_encNamesNew ? BST_CHECKED : BST_UNCHECKED, NULL);
            CheckRadioButton(*this, IDC_SYNCBOTHRADIO, IDC_SYNCDSTTOSRCRADIO, m_syncDir == BothWays ? IDC_SYNCBOTHRADIO : (m_syncDir == SrcToDst ? IDC_SYNCSRCTODSTRADIO : IDC_SYNCDSTTOSRCRADIO));
            DialogEnableWindow(IDC_RESETORIGINALARCH, m_syncDir != DstToSrc);
            SendDlgItemMessage(*this, IDC_SYNCDELETED, BM_SETCHECK, m_syncDeleted ? BST_CHECKED : BST_UNCHECKED, NULL);
            SendDlgItemMessage(*this, IDC_USE7ZEXT, BM_SETCHECK, m_7ZExt ? BST_CHECKED : BST_UNCHECKED, NULL);
            SendDlgItemMessage(*this, IDC_USEGPG, BM_SETCHECK, m_useGpg ? BST_CHECKED : BST_UNCHECKED, NULL);
            SendDlgItemMessage(*this, IDC_FAT, BM_SETCHECK, m_fat ? BST_CHECKED : BST_UNCHECKED, NULL);
            SendDlgItemMessage(*this, IDC_RESETORIGINALARCH, BM_SETCHECK, m_ResetOriginalArchAttr ? BST_CHECKED : BST_UNCHECKED, NULL);

            DialogEnableWindow(IDC_USE7ZEXT, !m_useGpg);

            AddToolTip(IDC_ONEWAY, L"if this is checked, changes in the encrypted folder are not synchronized back to the original folder!");
            AddToolTip(IDC_NOCOMPRESS, L"File masks, separated by '|' example: *.jpg|*.zip");
            AddToolTip(IDC_NOCRYPT, L"File masks, separated by '|' example: *.jpg|*.zip");
            AddToolTip(IDC_NOSYNC, L"File masks, separated by '|' example: *.jpg|*.zip");
            AddToolTip(IDC_SYNCDELETED, L"Delete files in original/encrypted if missing from encrypted/original (not applicable to Both Ways sync direction) ");
            AddToolTip(IDC_NEWNAMEENCRYPTION, L"New encryption method for names:\r\nUses less characters, but is harder to 'read' and not all filesystems support these characters.");
            AddToolTip(IDC_RESETORIGINALARCH, L"Reset archive attribute on original after succsssful encryption / copy");

            // the path edit control should work as a drop target for files and folders
            HWND hOrigPath    = GetDlgItem(hwndDlg, IDC_ORIGPATH);
            m_pDropTargetOrig = new CFileDropTarget(hOrigPath);
            RegisterDragDrop(hOrigPath, m_pDropTargetOrig);
            FORMATETC ftEtc = {0};
            ftEtc.cfFormat  = CF_TEXT;
            ftEtc.dwAspect  = DVASPECT_CONTENT;
            ftEtc.lindex    = -1;
            ftEtc.tymed     = TYMED_HGLOBAL;
            m_pDropTargetOrig->AddSuportedFormat(ftEtc);
            ftEtc.cfFormat = CF_HDROP;
            m_pDropTargetOrig->AddSuportedFormat(ftEtc);
            SHAutoComplete(GetDlgItem(*this, IDC_ORIGPATH), SHACF_FILESYSTEM | SHACF_AUTOSUGGEST_FORCE_ON);

            HWND hCryptPath    = GetDlgItem(hwndDlg, IDC_CRYPTPATH);
            m_pDropTargetCrypt = new CFileDropTarget(hCryptPath);
            RegisterDragDrop(hCryptPath, m_pDropTargetCrypt);
            ftEtc.cfFormat = CF_TEXT;
            ftEtc.dwAspect = DVASPECT_CONTENT;
            ftEtc.lindex   = -1;
            ftEtc.tymed    = TYMED_HGLOBAL;
            m_pDropTargetCrypt->AddSuportedFormat(ftEtc);
            ftEtc.cfFormat = CF_HDROP;
            m_pDropTargetCrypt->AddSuportedFormat(ftEtc);
            SHAutoComplete(GetDlgItem(*this, IDC_CRYPTPATH), SHACF_FILESYSTEM | SHACF_AUTOSUGGEST_FORCE_ON);
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
            auto buf   = GetDlgItemText(IDC_ORIGPATH);
            m_origPath = buf.get();
            m_origPath.erase(m_origPath.find_last_not_of(L" \n\r\t\\") + 1);

            buf         = GetDlgItemText(IDC_CRYPTPATH);
            m_cryptPath = buf.get();
            m_cryptPath.erase(m_cryptPath.find_last_not_of(L" \n\r\t\\") + 1);

            buf         = GetDlgItemText(IDC_NOCOMPRESS);
            m_cryptOnly = buf.get();
            m_cryptOnly.erase(m_cryptOnly.find_last_not_of(L" \n\r\t\\") + 1);

            buf        = GetDlgItemText(IDC_NOCRYPT);
            m_copyOnly = buf.get();
            m_copyOnly.erase(m_copyOnly.find_last_not_of(L" \n\r\t\\") + 1);

            buf      = GetDlgItemText(IDC_NOSYNC);
            m_noSync = buf.get();
            m_noSync.erase(m_noSync.find_last_not_of(L" \n\r\t\\") + 1);

            buf            = GetDlgItemText(IDC_COMPRESSSIZE);
            m_compressSize = _wtoi(buf.get());

            buf                 = GetDlgItemText(IDC_PASSWORD);
            m_password          = buf.get();
            buf                 = GetDlgItemText(IDC_PASSWORD2);
            std::wstring retype = buf.get();
            if (m_password != retype)
            {
                ::MessageBox(*this, L"password do not match!\nPlease reenter the password.", L"Password mismatch", MB_ICONERROR);
                return 0;
            }
            if ((m_password.find_first_of(L'\"') != std::string::npos) ||
                (m_password.find_first_of(L'\'') != std::string::npos) ||
                (m_password.find_first_of(L'\\') != std::string::npos) ||
                (m_password.find_first_of(L'/') != std::string::npos))
            {
                ::MessageBox(*this, L"password must not contain the following chars:\n\" \' / \\", L"Illegal Password char", MB_ICONERROR);
                return 0;
            }

            std::wstring origPath  = m_origPath;
            std::wstring cryptPath = m_cryptPath;
            CreateDirectory(m_origPath.c_str(), nullptr);
            CreateDirectory(m_cryptPath.c_str(), nullptr);
            std::transform(origPath.begin(), origPath.end(), origPath.begin(), ::towlower);
            std::transform(cryptPath.begin(), cryptPath.end(), cryptPath.begin(), ::towlower);

            if (origPath == cryptPath)
            {
                ::MessageBox(*this, L"source and destination are identical!\nPlease choose different folders.", L"Paths invalid", MB_ICONERROR);
                return 0;
            }

            if (((origPath.size() > cryptPath.size()) && (origPath.substr(0, cryptPath.size()) == cryptPath) && (origPath[cryptPath.size()] == '\\')) ||
                ((cryptPath.size() > origPath.size()) && (cryptPath.substr(0, origPath.size()) == origPath) && (cryptPath[origPath.size()] == '\\')))
            {
                ::MessageBox(*this, L"source and destination point to the same tree\nmake sure that one folder is not part of the other.", L"Paths invalid", MB_ICONERROR);
                return 0;
            }

            m_encNames = !!SendDlgItemMessage(*this, IDC_ENCNAMES, BM_GETCHECK, 0, NULL);
            m_encNamesNew = !!SendDlgItemMessage(*this, IDC_NEWNAMEENCRYPTION, BM_GETCHECK, 0, NULL);
            m_syncDir  = BothWays;
            if (IsDlgButtonChecked(*this, IDC_SYNCDSTTOSRCRADIO))
                m_syncDir = DstToSrc;
            if (IsDlgButtonChecked(*this, IDC_SYNCSRCTODSTRADIO))
                m_syncDir = SrcToDst;
            m_7ZExt   = !!SendDlgItemMessage(*this, IDC_USE7ZEXT, BM_GETCHECK, 0, NULL);
            m_syncDeleted   = !!SendDlgItemMessage(*this, IDC_SYNCDELETED, BM_GETCHECK, 0, NULL);
            m_useGpg = !!SendDlgItemMessage(*this, IDC_USEGPG, BM_GETCHECK, 0, NULL);
            m_fat     = !!SendDlgItemMessage(*this, IDC_FAT, BM_GETCHECK, 0, NULL);
            m_ResetOriginalArchAttr = !!SendDlgItemMessage(*this, IDC_RESETORIGINALARCH, BM_GETCHECK, 0, NULL);
            if (m_useGpg && m_password.empty())
            {
                ::MessageBox(*this, L"empty passwords are not allowed when using GPG for encryption!", L"empty password", MB_ICONERROR);
                return 0;
            }
        }
            // fall through
        case IDCANCEL:
            EndDialog(*this, id);
            break;
        case IDC_BROWSEORIG:
        {
            CBrowseFolder browse;

            auto path    = GetDlgItemText(IDC_ORIGPATH);
            auto pathBuf = std::make_unique<WCHAR[]>(MAX_PATH_NEW);
            wcscpy_s(pathBuf.get(), MAX_PATH_NEW, path.get());
            browse.SetInfo(_T("Select path to search"));
            if (browse.Show(*this, pathBuf.get(), MAX_PATH_NEW, m_origPath.c_str()) == CBrowseFolder::RetVal::Ok)
            {
                SetDlgItemText(*this, IDC_ORIGPATH, pathBuf.get());
                m_origPath = pathBuf.get();
            }
        }
        break;
        case IDC_BROWSECRYPT:
        {
            CBrowseFolder browse;

            auto path    = GetDlgItemText(IDC_CRYPTPATH);
            auto pathBuf = std::make_unique<WCHAR[]>(MAX_PATH_NEW);
            wcscpy_s(pathBuf.get(), MAX_PATH_NEW, path.get());
            browse.SetInfo(_T("Select path to search"));
            if (browse.Show(*this, pathBuf.get(), MAX_PATH_NEW, m_cryptPath.c_str()) == CBrowseFolder::RetVal::Ok)
            {
                SetDlgItemText(*this, IDC_CRYPTPATH, pathBuf.get());
                m_cryptPath = pathBuf.get();
            }
        }
        break;
        case IDC_USEGPG:
            m_useGpg = !m_useGpg;
            DialogEnableWindow(IDC_USE7ZEXT, !m_useGpg);
            break;
        case IDC_ENCNAMES:
            DialogEnableWindow(IDC_NEWNAMEENCRYPTION, SendDlgItemMessage(*this, IDC_ENCNAMES, BM_GETCHECK, 0, NULL));
            break;
        case IDC_SYNCBOTHRADIO:
        case IDC_SYNCSRCTODSTRADIO:
        case IDC_SYNCDSTTOSRCRADIO:
            DialogEnableWindow(IDC_RESETORIGINALARCH, id != IDC_SYNCDSTTOSRCRADIO);
            break;
    }
    return 1;
}
