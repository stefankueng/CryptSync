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
#include "CmdLineParser.h"
#include "TrayWindow.h"
#include "Ignores.h"
#include "PathUtils.h"
#include "CircularLog.h"
#include "resource.h"

constexpr auto MAX_LOADSTRING = 100;

// Global Variables:
HINSTANCE hInst;                         // current instance
TCHAR     szTitle[MAX_LOADSTRING];       // The title bar text
TCHAR     szWindowClass[MAX_LOADSTRING]; // the main window class name
CPairs    g_pairs;

// Forward declarations of functions included in this code module:
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK About(HWND, UINT, WPARAM, LPARAM);

std::wstring GetMutexID()
{
    std::wstring       t;
    CAutoGeneralHandle token;
    const BOOL         result = OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, token.GetPointer());
    if (result)
    {
        DWORD len = 0;
        GetTokenInformation(token, TokenStatistics, nullptr, 0, &len);
        if (len >= sizeof(TOKEN_STATISTICS))
        {
            auto data = std::make_unique<BYTE[]>(len);
            GetTokenInformation(token, TokenStatistics, data.get(), len, &len);
            const LUID uid      = reinterpret_cast<PTOKEN_STATISTICS>(data.get())->AuthenticationId;
            wchar_t    buf[100] = {0};
            swprintf_s(buf, L"{81C34844-03AC-4DAA-865B-BC51F07F7F9E}-%08x%08x", uid.HighPart, uid.LowPart);
            t = buf;
        }
    }
    return t;
}

int APIENTRY _tWinMain(HINSTANCE hInstance,
                       HINSTANCE hPrevInstance,
                       LPTSTR    lpCmdLine,
                       int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(nCmdShow);

    SetDllDirectory(L"");
    OleInitialize(nullptr);
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    LoadLibrary(L"riched32.dll");

    CCmdLineParser parser(lpCmdLine);
    if (parser.HasKey(L"?") || parser.HasKey(L"help"))
    {
        std::wstring sInfo = L"/src        : path to source folder with original content\n"
                             L"/dst        : path to encrpyted folder\n"
                             L"/pw         : password for encryption\n"
                             L"/ncp        : encrypt only/don't compress pattern\n"
                             L"/cpy        : copy only pattern\n"
                             L"/nsy        : don't sync pattern\n"
                             L"/encnames   : encrypt file and folder names\n"
                             L"/mirror     : mirror only from src to dst\n"
                             L"/mirrorback : mirror from dst to src\n"
                             L"/decryptonly: mirrors from dst to src, but does not mirror deletions\n"
                             L"/use7z      : use .7z instead of .cryptsync extension\n"
                             L"/useGPG     : use GnuPG encryption\n"
                             L"/fat        : use FAT write time accuracy (2s)\n"
                             L"/ignore     : ignore patterns\n"
                             L"/compress   : max size in MB a file is compressed. Larger\n"
                             L"              files are only encrypted\n"
                             L"\n"
                             L"/syncall    : syncs all set up pairs and then exists\n"
                             L"/progress   : shows a progress dialog while syncing\n"
                             L"/logpath    : path to a logfile\n"
                             L"/maxlog     : maximum number of lines the logfile can have\n"
                             L"/tray       : start in background without showing a dialog first\n\n"
                             L"the %ERRORLEVEL% is set to a bitmask on return, or zero on success:\n"
                             L"1: Cancelled\n"
                             L"2: Access denied\n"
                             L"4: en/decryption error\n"
                             L"8: copy error";
        MessageBox(nullptr, sInfo.c_str(), L"CryptSync Command Line Options", MB_ICONINFORMATION);
        return 1;
    }

    std::wstring lp     = parser.HasVal(L"logpath") ? parser.GetVal(L"logpath") : L"";
    int          maxlog = parser.HasVal(L"maxlog") ? parser.GetLongVal(L"maxlog") : 10000;

    if (lp.empty())
    {
        // in case CryptSync was installed with the installer, there's a registry
        // entry made by the installer.
        HKEY subKey = nullptr;
        LONG result = RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"Software\\CryptSync", 0, KEY_READ, &subKey);
        if (result != ERROR_SUCCESS)
        {
            // CryptSync was not installed, which means it's run as a portable app:
            // use the same directory as the exe is in to store the application data
            DWORD len       = 0;
            DWORD bufferLen = MAX_PATH; // MAX_PATH is not the limit here!
            auto  path      = std::make_unique<wchar_t[]>(bufferLen);
            do
            {
                bufferLen += MAX_PATH; // MAX_PATH is not the limit here!
                path = std::unique_ptr<wchar_t[]>(new wchar_t[bufferLen]);
                len  = GetModuleFileName(hInst, path.get(), bufferLen);
            } while (len == bufferLen);
            std::wstring sPath = path.get();
            sPath              = sPath.substr(0, sPath.find_last_of('\\'));
            lp                 = CPathUtils::GetLongPathname(sPath);
            lp += L"\\CryptSync.log";
        }
        else
        {
            RegCloseKey(subKey);
            // CryptSync is installed: we must not store the application data
            // in the same directory as the exe is but in %APPDATA%\CryptSync instead
            WCHAR outPath[MAX_PATH] = {0};
            if (SUCCEEDED(SHGetFolderPath(NULL, CSIDL_APPDATA | CSIDL_FLAG_CREATE, NULL, SHGFP_TYPE_CURRENT, outPath)))
            {
                lp = outPath;
                lp += L"\\CryptSync";
                lp = CPathUtils::GetLongPathname(lp);
                lp += L"\\CryptSync.log";
            }
        }
    }

    CCircularLog::Instance().Init(lp, maxlog);
    CCircularLog::Instance()(L"INFO:    Starting CryptSync");

    if (parser.HasVal(L"src") && parser.HasVal(L"dst"))
    {
        std::wstring src          = parser.GetVal(L"src");
        std::wstring dst          = parser.GetVal(L"dst");
        std::wstring pw           = parser.HasVal(L"pw") ? parser.GetVal(L"pw") : L"";
        std::wstring ncp          = parser.HasVal(L"ncp") ? parser.GetVal(L"ncp") : L"";
        std::wstring cpy          = parser.HasVal(L"cpy") ? parser.GetVal(L"cpy") : L"";
        std::wstring nsy          = parser.HasVal(L"nsy") ? parser.GetVal(L"nsy") : L"";
        bool         encnames     = !!parser.HasKey(L"encnames");
        bool         mirror       = !!parser.HasKey(L"mirror");
        bool         mirrorback   = !!parser.HasKey(L"mirrorback");
        bool         decryptonly  = !!parser.HasKey(L"decryptonly");
        bool         use7Z        = !!parser.HasKey(L"use7z");
        bool         useGpg       = !!parser.HasKey(L"useGPG");
        bool         fat          = !!parser.HasKey(L"fat");
        int          compresssize = _wtoi(parser.GetVal(L"compresssize"));
        std::wstring ign          = parser.HasVal(L"ignore") ? parser.GetVal(L"ignore") : L"";

        CIgnores::Instance().Reload();
        if (!ign.empty())
            CIgnores::Instance().Reload(ign);

        CPairs pair;
        pair.clear();
        SyncDir syncDir = BothWays;
        if (mirror && !mirrorback)
            syncDir = SrcToDst;
        if (mirrorback && !mirror)
            syncDir = DstToSrc;
        pair.AddPair(true, src, dst, pw, ncp, cpy, nsy, compresssize, encnames, syncDir, use7Z, useGpg, fat);
        CFolderSync foldersync;
        if (decryptonly)
            foldersync.DecryptOnly(true);
        const auto ret = foldersync.SyncFoldersWait(pair, parser.HasKey(L"progress") ? GetDesktopWindow() : nullptr);
        CCircularLog::Instance()(L"INFO:    exiting CryptSync");
        CCircularLog::Instance().Save();
        return ret;
    }
    if (parser.HasKey(L"syncall"))
    {
        CIgnores::Instance().Reload();

        CPairs      pair;
        CFolderSync foldersync;
        const auto  ret = foldersync.SyncFoldersWait(pair, parser.HasKey(L"progress") ? GetDesktopWindow() : nullptr);
        CCircularLog::Instance()(L"INFO:    exiting CryptSync");
        CCircularLog::Instance().Save();
        return ret;
    }

    MSG msg;

    HANDLE hReloadProtection = ::CreateMutex(nullptr, FALSE, GetMutexID().c_str());
    if ((!hReloadProtection) || (GetLastError() == ERROR_ALREADY_EXISTS))
    {
        // An instance of CryptSync is already running
        CoUninitialize();
        OleUninitialize();
        if (hReloadProtection)
            CloseHandle(hReloadProtection);
        CCircularLog::Instance()(L"INFO:    An instance of CryptSync is already running - exiting");
        CCircularLog::Instance().Save();
        return 0;
    }

    CTrayWindow trayWindow(hInstance);
    trayWindow.ShowDialogImmediately(!parser.HasKey(L"tray"));

    if (trayWindow.RegisterAndCreateWindow())
    {
        HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_CryptSync));
        // Main message loop:
        while (GetMessage(&msg, nullptr, 0, 0))
        {
            if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
        return static_cast<int>(msg.wParam);
    }

    CoUninitialize();
    OleUninitialize();
    CloseHandle(hReloadProtection);
    CCircularLog::Instance()(L"INFO:    exiting CryptSync");
    CCircularLog::Instance().Save();
    return 1;
}
