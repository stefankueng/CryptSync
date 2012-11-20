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
#include "TextDlg.h"
#include <string>

CTextDlg::CTextDlg(HWND hParent)
    : m_hParent(hParent)
{
}

CTextDlg::~CTextDlg(void)
{
}

LRESULT CTextDlg::DlgFunc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
        {
            InitDialog(hwndDlg, IDI_CryptSync);

            // initialize the controls
            SetDlgItemText(hwndDlg, IDC_RICHTEXT, m_text.c_str());

            m_resizer.Init(hwndDlg);
            m_resizer.AddControl(hwndDlg, IDC_RICHTEXT, RESIZER_TOPLEFTBOTTOMRIGHT);
            m_resizer.AddControl(hwndDlg, IDOK, RESIZER_BOTTOMRIGHT);
            if (m_Dwm.IsDwmCompositionEnabled())
                m_resizer.ShowSizeGrip(false);
        }
        return TRUE;
    case WM_COMMAND:
        return DoCommand(LOWORD(wParam));
    case WM_SIZE:
        {
            m_resizer.DoResize(LOWORD(lParam), HIWORD(lParam));
        }
        break;
    case WM_GETMINMAXINFO:
        {
            MINMAXINFO * mmi = (MINMAXINFO*)lParam;
            mmi->ptMinTrackSize.x = m_resizer.GetDlgRect()->right;
            mmi->ptMinTrackSize.y = m_resizer.GetDlgRect()->bottom;
        }
        break;
    default:
        return FALSE;
    }
    return FALSE;
}

LRESULT CTextDlg::DoCommand(int id)
{
    switch (id)
    {
    case IDOK:
    case IDCANCEL:
        EndDialog(*this, id);
        break;
    }
    return 1;
}

