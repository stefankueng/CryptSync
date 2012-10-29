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
#include "StdAfx.h"
#include "FolderSync.h"
#include "DirFileEnum.h"
#include "StringUtils.h"
#include "DebugOutput.h"

#include <process.h>


CFolderSync::CFolderSync(void)
{
}


CFolderSync::~CFolderSync(void)
{
}

void CFolderSync::SyncFolders( const PairVector& pv )
{
    m_pairs = pv;
    unsigned int threadId = 0;
    _beginthreadex(NULL, 0, SyncFolderThreadEntry, this, 0, &threadId);
}

unsigned int CFolderSync::SyncFolderThreadEntry(void* pContext)
{
    ((CFolderSync*)pContext)->SyncFolderThread();
    return 0;
}

void CFolderSync::SyncFolderThread()
{
    for (auto it = m_pairs.cbegin(); it != m_pairs.cend(); ++it)
    {
        SyncFolder(*it);
    }
}

void CFolderSync::SyncFolder( const PairTuple& pt )
{
    std::map<std::wstring,FileData> origFileList  = GetFileList(std::get<0>(pt), std::get<2>(pt));
    std::map<std::wstring,FileData> cryptFileList = GetFileList(std::get<1>(pt), std::get<2>(pt));

    for (auto it = origFileList.cbegin(); it != origFileList.cend(); ++it)
    {
        auto cryptit = cryptFileList.find(it->first);
        if (cryptit == cryptFileList.end())
        {
            // file does not exist in the encrypted folder:
            // encrypt the file
            CTraceToOutputDebugString::Instance()(_T(__FUNCTION__) _T(": file %s does not exist in encrypted folder\n"), it->first.c_str());
            size_t slashpos = it->first.find_last_of('\\');
            std::wstring fname = it->first;
            if (slashpos != std::string::npos)
                fname = it->first.substr(slashpos + 1);
            std::wstring cryptname = GetEncryptedFilename(fname, std::get<2>(pt));
            std::wstring cryptpath = std::get<1>(pt) + L"\\" + ((slashpos != std::string::npos) ? it->first.substr(0, slashpos) : L"");
            cryptpath = cryptpath + L"\\" + cryptname;
            std::wstring origpath = std::get<0>(pt) + L"\\" + it->first;
            EncryptFile(origpath, cryptpath, std::get<2>(pt));
        }
        else
        {
            LONG cmp = CompareFileTime(&it->second.ft, &cryptit->second.ft);
            if (cmp < 0)
            {
                // original file is older than the encrypted file
                // decrypt the file
            }
            else if (cmp > 0)
            {
                // encrypted file is older than the original file
                // encrypt the file
                CTraceToOutputDebugString::Instance()(_T(__FUNCTION__) _T(": file %s is newer than its encrypted partner\n"), it->first.c_str());
                size_t slashpos = it->first.find_last_of('\\');
                std::wstring fname = it->first;
                if (slashpos != std::string::npos)
                    fname = it->first.substr(slashpos + 1);
                std::wstring cryptname = GetEncryptedFilename(fname, std::get<2>(pt));
                std::wstring cryptpath = std::get<1>(pt) + L"\\" + ((slashpos != std::string::npos) ? it->first.substr(0, slashpos) : L"");
                cryptpath = cryptpath + L"\\" + cryptname;
                std::wstring origpath = std::get<0>(pt) + L"\\" + it->first;
                EncryptFile(origpath, cryptpath, std::get<2>(pt));
            }
            else if (cmp == 0)
            {
                // files are identical (have the same last-write-time):
                // nothing to do.
                CTraceToOutputDebugString::Instance()(_T(__FUNCTION__) _T(": nothing to do for file %s\n"), it->first.c_str());
            }
        }
    }
    // now go through the encrypted file list and if there's a file that's not in the original file list,
    // decrypt it
    for (auto it = cryptFileList.cbegin(); it != cryptFileList.cend(); ++it)
    {
        auto origit = origFileList.find(it->first);
        if (origit == origFileList.end())
        {
            // file does not exist in the original folder:
            // decrypt the file
            CTraceToOutputDebugString::Instance()(_T(__FUNCTION__) _T(": decrypt file %s to %s\n"), it->first.c_str(), std::get<0>(pt).c_str());
        }
    }
}

std::map<std::wstring,FileData> CFolderSync::GetFileList( const std::wstring& path, const std::wstring& password ) const
{
    CDirFileEnum enumerator(path);

    std::map<std::wstring,FileData> filelist;
    std::wstring filepath;
    bool isDir = false;
    while (enumerator.NextFile(filepath, &isDir, true))
    {
        if (isDir)
            continue;

        FileData fd;

        fd.ft = enumerator.GetLastWriteTime();
        if ((fd.ft.dwLowDateTime == 0) && (fd.ft.dwHighDateTime == 0))
            fd.ft = enumerator.GetCreateTime();

        std::wstring relpath = filepath.substr(path.size()+1);
        size_t slashPos = relpath.find_last_of('\\');
        if (slashPos != std::string::npos)
            fd.filename = relpath.substr(slashPos+1);
        else
            fd.filename = relpath;

        std::wstring decryptedFileName = GetDecryptedFilename(fd.filename, password);
        if (decryptedFileName != fd.filename)
        {
            if (slashPos != std::string::npos)
                relpath = relpath.substr(0, slashPos) + L"\\" + decryptedFileName;
            else
                relpath = decryptedFileName;
        }

        filelist[relpath] = fd;
    }

    return filelist;
}

bool CFolderSync::EncryptFile( const std::wstring& orig, const std::wstring& crypt, const std::wstring& password )
{
    CTraceToOutputDebugString::Instance()(_T(__FUNCTION__) _T(": encrypt file %s to %s\n"), orig.c_str(), crypt.c_str());
    return true;
}

bool CFolderSync::DecryptFile( const std::wstring& orig, const std::wstring& crypt, const std::wstring& password )
{
    CTraceToOutputDebugString::Instance()(_T(__FUNCTION__) _T(": decrypt file %s to %s\n"), orig.c_str(), crypt.c_str());
    return true;
}

std::wstring CFolderSync::GetDecryptedFilename( const std::wstring& filename, const std::wstring& password ) const
{
    bool bResult = true;
    std::wstring decryptName = filename;
    std::wstring fname = filename.substr(0, filename.find_last_of('.'));

    std::unique_ptr<BYTE[]> strIn(new BYTE[fname.size()*sizeof(WCHAR) + 1]);
    if (!CStringUtils::FromHexString(fname, strIn.get()))
        return filename;

    HCRYPTPROV hProv = NULL;
    // Get handle to user default provider.
    if (CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_FULL, 0))
    {
        HCRYPTHASH hHash = NULL;
        // Create hash object.
        if (CryptCreateHash(hProv, CALG_MD5, 0, 0, &hHash))
        {
            // Hash password string.
            DWORD dwLength = sizeof(WCHAR)*password.size();
            if (CryptHashData(hHash, (BYTE *)password.c_str(), dwLength, 0))
            {
                HCRYPTKEY hKey = NULL;
                // Create block cipher session key based on hash of the password.
                if (CryptDeriveKey(hProv, CALG_RC4, hHash, CRYPT_EXPORTABLE, &hKey))
                {
                    dwLength = fname.size() * sizeof(WCHAR) + 1024; // 1024 bytes should be enough for padding
                    std::unique_ptr<WCHAR[]> tempstring(new WCHAR[dwLength]);
                    // copy encrypted password to temporary WCHAR
                    wcscpy_s(tempstring.get(), fname.size()+1024, fname.c_str());
                    if (!CryptDecrypt(hKey, 0, true, 0, (BYTE *)tempstring.get(), &dwLength))
                        bResult = false;
                    CryptDestroyKey(hKey);  // Release provider handle.

                    decryptName = std::wstring(tempstring.get(), dwLength/sizeof(WCHAR));
                }
                else
                {
                    bResult = false;
                }
            }
            else
            {
                bResult = false;
            }
            CryptDestroyHash(hHash); // Destroy session key.
        }
        else
        {
            bResult = false;
        }
        CryptReleaseContext(hProv, 0);
    }
    if (bResult)
    {
        if (decryptName.empty() || (decryptName[0] != '*'))
            return filename;
        decryptName = decryptName.substr(1);    // cut off the starting '*'
    }

    return decryptName;
}

std::wstring CFolderSync::GetEncryptedFilename( const std::wstring& filename, const std::wstring& password ) const
{
    std::wstring encryptFilename = filename;

    bool bResult = true;

    HCRYPTPROV hProv = NULL;
    // Get handle to user default provider.
    if (CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_FULL, 0))
    {
        HCRYPTHASH hHash = NULL;
        // Create hash object.
        if (CryptCreateHash(hProv, CALG_MD5, 0, 0, &hHash))
        {
            // Hash password string.
            DWORD dwLength = sizeof(WCHAR)*password.size();
            if (CryptHashData(hHash, (BYTE *)password.c_str(), dwLength, 0))
            {
                // Create block cipher session key based on hash of the password.
                HCRYPTKEY hKey = NULL;
                if (CryptDeriveKey(hProv, CALG_RC4, hHash, CRYPT_EXPORTABLE, &hKey))
                {
                    // Determine number of bytes to encrypt at a time.
                    dwLength = sizeof(WCHAR)*filename.size();

                    std::unique_ptr<BYTE[]> buffer(new BYTE[dwLength+1024]);
                    std::wstring starname = L"*";
                    starname += filename;
                    memcpy(buffer.get(), starname.c_str(), dwLength);
                    // Encrypt data
                    if (CryptEncrypt(hKey, 0, true, 0, buffer.get(), &dwLength, dwLength+1024))
                    {
                        encryptFilename = CStringUtils::ToHexWString(buffer.get(), dwLength);
                        encryptFilename += L".cryptsync";
                    }
                    else
                    {
                        bResult = false;
                    }
                    CryptDestroyKey(hKey);  // Release provider handle.
                }
                else
                {
                    bResult = false;
                }
            }
            else
            {
                bResult = false;
            }
            CryptDestroyHash(hHash);
        }
        else
        {
            bResult = false;
        }
        CryptReleaseContext(hProv, 0);
    }
    if (bResult)
        return encryptFilename;
    return filename;
}
