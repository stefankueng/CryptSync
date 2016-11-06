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

#include "BaseDialog.h"
#include "hyperlink.h"
#include "AeroControls.h"

/**
 * about dialog.
 */
class CUpdateDlg : public CDialog
{
public:
    CUpdateDlg(HWND hParent);
    ~CUpdateDlg(void);

    static bool             CheckNewer();
protected:
    LRESULT CALLBACK        DlgFunc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
    LRESULT                 DoCommand(int id);
    static std::wstring     GetTempFilePath();

private:
    HWND                    m_hParent;
    CHyperLink              m_link;
    AeroControlBase         m_aerocontrols;
};
