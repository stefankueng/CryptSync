// CryptSync - A folder sync tool with encryption

// Copyright (C) 2018 - Stefan Kueng

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
#include "Helper.h"

#include <Shlwapi.h>


bool CreateRecursiveDirectory(const std::wstring& path)
{
    if (path.empty() || PathIsRoot(path.c_str()))
        return false;

    auto ret = CreateDirectory(path.c_str(), nullptr);
    if (ret == FALSE)
    {
        if (GetLastError() != ERROR_ALREADY_EXISTS)//on Win10 with webdav mounted drives GetLastError return with ERROR_FILE_NOT_FOUND(2) (or  Error not set)
        {
            if (CreateRecursiveDirectory(path.substr(0, path.find_last_of('\\'))))
            {
                // some file systems (e.g. webdav mounted drives) take time until
                // a dir is properly created. So we try a few times with a wait in between
                // to create the sub dir after just having created the parent dir.
                int retrycount = 5;
                do
                {
                    ret = CreateDirectory(path.c_str(), nullptr);
                    if (ret == FALSE)
                        Sleep(50);
                } while (retrycount-- && (ret == FALSE));
            }
        }
    }
    return ret != FALSE;
}
