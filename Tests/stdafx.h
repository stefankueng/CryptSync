//
// pch.h
// Header for standard system include files.
//

#pragma once
// Including SDKDDKVer.h defines the highest available Windows platform.

// If you wish to build your application for a previous Windows platform, include WinSDKVer.h and
// set the _WIN32_WINNT macro to the platform you wish to support before including SDKDDKVer.h.

#include <SDKDDKVer.h>

// Windows Header Files:
#include <windows.h>
#include <Shlwapi.h>

// C RunTime Header Files
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>

#pragma warning(push)
#pragma warning(disable : 4458) // declaration of 'xxx' hides class member
#include <GdiPlus.h>
#pragma warning(pop)

#pragma comment(linker, "\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#include "../src/Pairs.h"

extern CPairs g_pairs;

#define TRAY_WM_MESSAGE (WM_APP + 1)
#define WM_THREADENDED  (WM_APP + 2)
#define WM_PROGRESS     (WM_APP + 3)

#define DEBUGOUTPUTREGPATH L"Software\\CryptSync\\DebugOutputString"

#include "gtest/gtest.h"
