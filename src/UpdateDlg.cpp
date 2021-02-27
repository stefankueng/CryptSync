// CryptSync - A folder sync tool with encryption

// Copyright (C) 2012-2013, 2015, 2021 - Stefan Kueng

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
#include "UpdateDlg.h"
#include "Registry.h"
#include "version.h"
#include <string>
#include <Commdlg.h>
#include <time.h>
#include <fstream>

CUpdateDlg::CUpdateDlg(HWND hParent)
    : m_hParent(hParent)
{
}

CUpdateDlg::~CUpdateDlg()
{
}

LRESULT CUpdateDlg::DlgFunc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (uMsg)
    {
        case WM_INITDIALOG:
        {
            InitDialog(hwndDlg, IDI_CryptSync);
            // initialize the controls
            m_link.ConvertStaticToHyperlink(hwndDlg, IDC_WEBURL, _T("https://tools.stefankueng.com/CryptSync.html"));

            ExtendFrameIntoClientArea(static_cast<UINT>(-1), static_cast<UINT>(-1), static_cast<UINT>(-1), static_cast<UINT>(-1));
            m_aeroControls.SubclassControl(GetDlgItem(*this, IDC_INFOLABEL));
            m_aeroControls.SubclassControl(GetDlgItem(*this, IDC_INFOLABEL2));
            m_aeroControls.SubclassControl(GetDlgItem(*this, IDOK));
            m_aeroControls.SubclassControl(GetDlgItem(*this, IDC_WEBURL));
        }
            return TRUE;
        case WM_COMMAND:
            return DoCommand(LOWORD(wParam));
        default:
            return FALSE;
    }
}

LRESULT CUpdateDlg::DoCommand(int id)
{
    switch (id)
    {
        case IDOK:
            // fall through
        case IDCANCEL:
            EndDialog(*this, id);
            break;
    }
    return 1;
}

std::wstring CUpdateDlg::GetTempFilePath()
{
    DWORD len      = ::GetTempPath(0, nullptr);
    auto  tempPath = std::make_unique<TCHAR[]>(len + 1);
    auto  tempF    = std::make_unique<TCHAR[]>(len + 50);
    ::GetTempPath(len + 1, tempPath.get());
    std::wstring tempFile;
    ::GetTempFileName(tempPath.get(), TEXT("cm_"), 0, tempF.get());
    tempFile = std::wstring(tempF.get());
    //now create the tempfile, so that subsequent calls to GetTempFile() return
    //different filenames.
    HANDLE hFile = CreateFile(tempFile.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY, nullptr);
    CloseHandle(hFile);
    return tempFile;
}

bool CUpdateDlg::CheckNewer()
{
    bool bNewerAvailable = false;
    // check for newer versions
    if (CRegStdDWORD(_T("Software\\CryptSync\\CheckNewer"), TRUE) != FALSE)
    {
        time_t    now;
        struct tm ptm;

        time(&now);
        if ((now != 0) && (localtime_s(&ptm, &now) == 0))
        {
            int week = 0;
            // we don't calculate the real 'week of the year' here
            // because just to decide if we should check for an update
            // that's not needed.
            week = ptm.tm_yday / 7;

            CRegStdDWORD oldweek = CRegStdDWORD(_T("Software\\CryptSync\\CheckNewerWeek"), static_cast<DWORD>(-1));
            if (static_cast<DWORD>(oldweek) == -1)
                oldweek = week; // first start of CommitMonitor, no update check needed
            else
            {
                if (static_cast<DWORD>(week) != oldweek)
                {
                    oldweek = week;
                    //
                    std::wstring tempFile = GetTempFilePath();

                    CRegStdString checkUrlUser    = CRegStdString(_T("Software\\CryptSync\\UpdateCheckURL"), _T(""));
                    CRegStdString checkUrlMachine = CRegStdString(_T("Software\\CryptSync\\UpdateCheckURL"), _T(""), FALSE, HKEY_LOCAL_MACHINE);
                    std::wstring  sCheckURL       = static_cast<std::wstring>(checkUrlUser);
                    if (sCheckURL.empty())
                    {
                        sCheckURL = static_cast<std::wstring>(checkUrlMachine);
                        if (sCheckURL.empty())
                            sCheckURL = _T("https://github.com/stefankueng/CryptSync/raw/master/version.txt");
                    }
                    HRESULT res = URLDownloadToFile(nullptr, sCheckURL.c_str(), tempFile.c_str(), 0, nullptr);
                    if (res == S_OK)
                    {
                        std::ifstream file;
                        file.open(tempFile.c_str());
                        if (file.good())
                        {
                            char  line[1024];
                            char* pLine = line;
                            file.getline(line, sizeof(line));
                            int major = 0;
                            int minor = 0;
                            int micro = 0;
                            int build = 0;

                            major = atoi(pLine);
                            pLine = strchr(pLine, '.');
                            if (pLine)
                            {
                                pLine++;
                                minor = atoi(pLine);
                                pLine = strchr(pLine, '.');
                                if (pLine)
                                {
                                    pLine++;
                                    micro = atoi(pLine);
                                    pLine = strchr(pLine, '.');
                                    if (pLine)
                                    {
                                        pLine++;
                                        build = atoi(pLine);
                                    }
                                }
                            }
                            if (major > CS_VERMAJOR)
                                bNewerAvailable = true;
                            else if ((minor > CS_VERMINOR) && (major == CS_VERMAJOR))
                                bNewerAvailable = true;
                            else if ((micro > CS_VERMICRO) && (minor == CS_VERMINOR) && (major == CS_VERMAJOR))
                                bNewerAvailable = true;
                            else if ((build > CS_VERBUILD) && (micro == CS_VERMICRO) && (minor == CS_VERMINOR) && (major == CS_VERMAJOR))
                                bNewerAvailable = true;
                        }
                        file.close();
                    }
                    DeleteFile(tempFile.c_str());
                }
            }
        }
    }
    return bNewerAvailable;
}
