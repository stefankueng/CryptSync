// CryptSync - A folder sync tool with encryption

// Copyright (C) 2012-2013 - Stefan Kueng

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
#include "AeroControls.h"
#include "FileDropTarget.h"


/**
 * options dialog.
 */
class CPairAddDlg : public CDialog
{
public:
    CPairAddDlg(HWND hParent);
    ~CPairAddDlg(void);

    std::wstring            m_origpath;
    std::wstring            m_cryptpath;
    std::wstring            m_password;
    std::wstring            m_copyonly;
    std::wstring            m_nosync;
    bool                    m_encnames;
    bool                    m_oneway;
    bool                    m_7zExt;
    bool                    m_FAT;
protected:
    LRESULT CALLBACK        DlgFunc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
    LRESULT                 DoCommand(int id);

private:
    HWND                    m_hParent;
    AeroControlBase         m_aerocontrols;
    CFileDropTarget *       m_pDropTargetOrig;
    CFileDropTarget *       m_pDropTargetCrypt;
};
