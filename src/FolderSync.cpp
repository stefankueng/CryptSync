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
#include "UnicodeUtils.h"
#include "Ignores.h"
#include "CreateProcessHelper.h"
#include "SmartHandle.h"
#include "DebugOutput.h"

#include <process.h>
#include <shlobj.h>

#pragma comment(lib, "shell32.lib")

CFolderSync::CFolderSync(void)
    : m_parentWnd(NULL)
    , m_pProgDlg(NULL)
{
    m_sevenzip = L"%ProgramFiles%\\7-zip\\7z.exe";
    m_sevenzip = CStringUtils::ExpandEnvironmentStrings(m_sevenzip);
    if (!PathFileExists(m_sevenzip.c_str()))
    {
        m_sevenzip = L"%ProgramW6432%\\7-zip\\7z.exe";
        m_sevenzip = CStringUtils::ExpandEnvironmentStrings(m_sevenzip);
        if (!PathFileExists(m_sevenzip.c_str()))
        {
            // user does not have 7zip installed, use
            // our own copy
            wchar_t buf[1024] = {0};
            GetModuleFileName(NULL, buf, 1024);
            std::wstring dir = buf;
            dir = dir.substr(0, dir.find_last_of('\\'));
            m_sevenzip = dir + L"\\7z.exe";
        }
    }
}


CFolderSync::~CFolderSync(void)
{
}

void CFolderSync::SyncFolders( const PairVector& pv, HWND hWnd )
{
    CAutoWriteLock locker(m_guard);
    m_pairs = pv;
    m_parentWnd = hWnd;
    unsigned int threadId = 0;
    _beginthreadex(NULL, 0, SyncFolderThreadEntry, this, 0, &threadId);
}

unsigned int CFolderSync::SyncFolderThreadEntry(void* pContext)
{
    ((CFolderSync*)pContext)->SyncFolderThread();
    _endthreadex(0);
    return 0;
}

void CFolderSync::SyncFolderThread()
{
    PairVector pv;
    {
        CAutoReadLock locker(m_guard);
        pv = m_pairs;
    }
    if (m_parentWnd)
    {
        CoInitializeEx(0, COINIT_APARTMENTTHREADED);
        m_progress = 0;
        m_progressTotal = 1;
        m_pProgDlg = new CProgressDlg();
        m_pProgDlg->SetTitle(L"Syncing folders");
        m_pProgDlg->SetLine(0, L"scanning...");
        m_pProgDlg->SetProgress(m_progress, m_progressTotal);
        m_pProgDlg->ShowModal(m_parentWnd);
    }
    for (auto it = pv.cbegin(); it != pv.cend(); ++it)
    {
        SyncFolder(*it);
    }
    if (m_pProgDlg)
    {
        delete m_pProgDlg;
        m_pProgDlg = NULL;
        CoUninitialize();
    }
    PostMessage(m_parentWnd, WM_THREADENDED, 0, 0);
    m_parentWnd = NULL;
}


void CFolderSync::SyncFile( const std::wstring& path )
{
    if (PathIsDirectory(path.c_str()))
        return;
    PairTuple pt;
    CAutoReadLock locker(m_guard);
    for (auto it = m_pairs.cbegin(); it != m_pairs.cend(); ++it)
    {
        std::wstring s = std::get<0>(*it);
        if (path.size() > s.size())
        {
            if ((path.substr(0, s.size()) == s)&&(path[s.size()] == '\\'))
            {
                pt = *it;
                break;
            }
        }
        s = std::get<1>(*it);
        if (path.size() > s.size())
        {
            if ((path.substr(0, s.size()) == s)&&(path[s.size()] == '\\'))
            {
                pt = *it;
                break;
            }
        }
    }

    std::wstring orig  = std::get<0>(pt);
    std::wstring crypt = std::get<1>(pt);
    if (orig.empty() || crypt.empty())
        return;

    if ((orig.size() < path.size())&&(path.substr(0, orig.size()) == orig))
    {
        std::wstring filename = path.substr(path.find_last_of('\\')+1);
        filename = GetEncryptedFilename(filename, std::get<2>(pt), std::get<3>(pt));
        crypt = crypt + path.substr(orig.size());
        crypt = crypt.substr(0, crypt.find_last_of('\\')+1) + filename;
        orig = path;
    }
    else
    {
        std::wstring filename = path.substr(path.find_last_of('\\')+1);
        filename = GetDecryptedFilename(filename, std::get<2>(pt), std::get<3>(pt));
        orig = orig + path.substr(crypt.size());
        orig = orig.substr(0, orig.find_last_of('\\')+1) + filename;
        crypt = path;
    }

    WIN32_FILE_ATTRIBUTE_DATA fdataorig  = {0};
    WIN32_FILE_ATTRIBUTE_DATA fdatacrypt = {0};
    if ((!GetFileAttributesEx(orig.c_str(),  GetFileExInfoStandard, &fdataorig)) &&
        (GetLastError() == ERROR_ACCESS_DENIED))
        return;
    if ((!GetFileAttributesEx(crypt.c_str(), GetFileExInfoStandard, &fdatacrypt)) &&
        (GetLastError() == ERROR_ACCESS_DENIED))
        return;

    if ((fdataorig.ftLastWriteTime.dwLowDateTime == 0)&&(fdataorig.ftLastWriteTime.dwHighDateTime == 0)&&
        (orig==path))
    {
        // original file got deleted
        // delete the encrypted file
        SHFILEOPSTRUCT fop = {0};
        fop.wFunc = FO_DELETE;
        fop.fFlags = FOF_ALLOWUNDO|FOF_FILESONLY|FOF_NOCONFIRMATION|FOF_NO_CONNECTED_ELEMENTS|FOF_NOERRORUI|FOF_SILENT;
        std::unique_ptr<wchar_t[]> delbuf(new wchar_t[crypt.size()+2]);
        wcscpy_s(delbuf.get(), crypt.size()+2, crypt.c_str());
        delbuf[crypt.size()] = 0;
        delbuf[crypt.size()+1] = 0;
        fop.pFrom = delbuf.get();
        SHFileOperation(&fop);
        return;
    }
    else if ((fdatacrypt.ftLastWriteTime.dwLowDateTime == 0)&&(fdatacrypt.ftLastWriteTime.dwHighDateTime == 0)&&
             (crypt==path))
    {
        // encrypted file got deleted
        // delete the original file as well
        SHFILEOPSTRUCT fop = {0};
        fop.wFunc = FO_DELETE;
        fop.fFlags = FOF_ALLOWUNDO|FOF_FILESONLY|FOF_NOCONFIRMATION|FOF_NO_CONNECTED_ELEMENTS|FOF_NOERRORUI|FOF_SILENT;
        std::unique_ptr<wchar_t[]> delbuf(new wchar_t[orig.size()+2]);
        wcscpy_s(delbuf.get(), orig.size()+2, orig.c_str());
        delbuf[orig.size()] = 0;
        delbuf[orig.size()+1] = 0;
        fop.pFrom = delbuf.get();
        SHFileOperation(&fop);
        return;
    }

    LONG cmp = CompareFileTime(&fdataorig.ftLastWriteTime, &fdatacrypt.ftLastWriteTime);
    if (cmp < 0)
    {
        // original file is older than the encrypted file
        // decrypt the file
        FileData fd;
        fd.ft = fdatacrypt.ftLastWriteTime;
        DecryptFile(orig, crypt, std::get<2>(pt), fd);
    }
    else if (cmp > 0)
    {
        // encrypted file is older than the original file
        // encrypt the file
        FileData fd;
        fd.ft = fdataorig.ftLastWriteTime;
        EncryptFile(orig, crypt, std::get<2>(pt), fd);
    }
    else if (cmp == 0)
    {
        // files are identical (have the same last-write-time):
        // nothing to do.
    }
}

void CFolderSync::SyncFolder( const PairTuple& pt )
{
    CIgnores ignores;
    if (m_pProgDlg)
    {
        m_pProgDlg->SetLine(0, L"scanning...");
        m_pProgDlg->SetLine(2, L"");
        m_pProgDlg->SetProgress(m_progress, m_progressTotal);
    }
    std::map<std::wstring,FileData> origFileList  = GetFileList(std::get<0>(pt), std::get<2>(pt), std::get<3>(pt));
    std::map<std::wstring,FileData> cryptFileList = GetFileList(std::get<1>(pt), std::get<2>(pt), std::get<3>(pt));

    m_progressTotal += (origFileList.size() + cryptFileList.size());

    for (auto it = origFileList.cbegin(); it != origFileList.cend(); ++it)
    {
        if (m_pProgDlg)
        {
            m_pProgDlg->SetLine(0, L"syncing files");
            m_pProgDlg->SetLine(2, it->first.c_str(), true);
            m_pProgDlg->SetProgress(m_progress++, m_progressTotal);
            if (m_pProgDlg->HasUserCancelled())
                break;
        }

        if (ignores.IsIgnored(it->first))
            continue;
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
            std::wstring cryptname = GetEncryptedFilename(fname, std::get<2>(pt), std::get<3>(pt));
            std::wstring cryptpath = std::get<1>(pt) + L"\\" + ((slashpos != std::string::npos) ? it->first.substr(0, slashpos) : L"");
            cryptpath = cryptpath + L"\\" + cryptname;
            std::wstring origpath = std::get<0>(pt) + L"\\" + it->first;
            EncryptFile(origpath, cryptpath, std::get<2>(pt), it->second);
        }
        else
        {
            LONG cmp = CompareFileTime(&it->second.ft, &cryptit->second.ft);
            if (cmp < 0)
            {
                // original file is older than the encrypted file
                // decrypt the file
                size_t slashpos = it->first.find_last_of('\\');
                std::wstring fname = it->first;
                if (slashpos != std::string::npos)
                    fname = it->first.substr(slashpos + 1);
                std::wstring cryptname = GetEncryptedFilename(fname, std::get<2>(pt), std::get<3>(pt));
                std::wstring cryptpath = std::get<1>(pt) + L"\\" + ((slashpos != std::string::npos) ? it->first.substr(0, slashpos) : L"");
                cryptpath = cryptpath + L"\\" + cryptname;
                std::wstring origpath = std::get<0>(pt) + L"\\" + it->first;
                DecryptFile(origpath, cryptpath, std::get<2>(pt), it->second);
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
                std::wstring cryptname = GetEncryptedFilename(fname, std::get<2>(pt), std::get<3>(pt));
                std::wstring cryptpath = std::get<1>(pt) + L"\\" + ((slashpos != std::string::npos) ? it->first.substr(0, slashpos) : L"");
                cryptpath = cryptpath + L"\\" + cryptname;
                std::wstring origpath = std::get<0>(pt) + L"\\" + it->first;
                EncryptFile(origpath, cryptpath, std::get<2>(pt), it->second);
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
        if (m_pProgDlg)
        {
            m_pProgDlg->SetLine(0, L"syncing files");
            m_pProgDlg->SetLine(2, it->first.c_str(), true);
            m_pProgDlg->SetProgress(m_progress++, m_progressTotal);
            if (m_pProgDlg->HasUserCancelled())
                break;
        }

        if (ignores.IsIgnored(it->first))
            continue;
        auto origit = origFileList.find(it->first);
        if (origit == origFileList.end())
        {
            // file does not exist in the original folder:
            // decrypt the file
            CTraceToOutputDebugString::Instance()(_T(__FUNCTION__) _T(": decrypt file %s to %s\n"), it->first.c_str(), std::get<0>(pt).c_str());
            size_t slashpos = it->first.find_last_of('\\');
            std::wstring fname = it->first;
            if (slashpos != std::string::npos)
                fname = it->first.substr(slashpos + 1);
            std::wstring cryptname = it->second.filename;
            std::wstring cryptpath = std::get<1>(pt) + L"\\" + ((slashpos != std::string::npos) ? it->first.substr(0, slashpos) : L"");
            cryptpath = cryptpath + L"\\" + cryptname;
            std::wstring origpath = std::get<0>(pt) + L"\\" + it->first;
            if (!DecryptFile(origpath, cryptpath, std::get<2>(pt), it->second))
            {
                if (!it->second.filenameEncrypted)
                {
                    MoveFileEx(cryptpath.c_str(), origpath.c_str(), MOVEFILE_COPY_ALLOWED);
                }
            }
        }
    }
}

std::map<std::wstring,FileData> CFolderSync::GetFileList( const std::wstring& path, const std::wstring& password, bool encnames ) const
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

        std::wstring decryptedFileName = GetDecryptedFilename(fd.filename, password, encnames);
        fd.filenameEncrypted = (decryptedFileName != fd.filename);
        if (fd.filenameEncrypted)
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

bool CFolderSync::EncryptFile( const std::wstring& orig, const std::wstring& crypt, const std::wstring& password, const FileData& fd )
{
    CTraceToOutputDebugString::Instance()(_T(__FUNCTION__) _T(": encrypt file %s to %s\n"), orig.c_str(), crypt.c_str());

    size_t slashpos = crypt.find_last_of('\\');
    if (slashpos == std::string::npos)
        return false;
    std::wstring targetfolder = crypt.substr(0, slashpos);
    std::wstring cryptname = crypt.substr(slashpos+1);
    int buflen = orig.size() + crypt.size() + password.size() + 1000;
    std::unique_ptr<wchar_t[]> cmdlinebuf(new wchar_t[buflen]);
    swprintf_s(cmdlinebuf.get(), buflen, L"\"%s\" a -t7z \"%s\" \"%s\" -mx9 -p%s -mhe=on", m_sevenzip.c_str(), cryptname.c_str(), orig.c_str(), password.c_str());
    bool bRet = Run7Zip(cmdlinebuf.get(), targetfolder);
    if (!bRet)
    {
        SHCreateDirectory(NULL, targetfolder.c_str());
        bRet = Run7Zip(cmdlinebuf.get(), targetfolder);
    }
    if (bRet)
    {
        // set the file timestamp
        int retry = 5;
        do 
        {
            CAutoFile hFile = CreateFile(crypt.c_str(), GENERIC_WRITE|GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
            if (hFile.IsValid())
            {
                bRet = !!SetFileTime(hFile, NULL, NULL, &fd.ft);
            }
            else
                bRet = false;
            if (!bRet)
                Sleep(200);
        } while (!bRet && (retry-- > 0));
    }
    return bRet;
}

bool CFolderSync::DecryptFile( const std::wstring& orig, const std::wstring& crypt, const std::wstring& password, const FileData& fd )
{
    CTraceToOutputDebugString::Instance()(_T(__FUNCTION__) _T(": decrypt file %s to %s\n"), orig.c_str(), crypt.c_str());
    size_t slashpos = orig.find_last_of('\\');
    if (slashpos == std::string::npos)
        return false;
    std::wstring targetfolder = orig.substr(0, slashpos);
    int buflen = orig.size() + crypt.size() + password.size() + 1000;
    std::unique_ptr<wchar_t[]> cmdlinebuf(new wchar_t[buflen]);
    swprintf_s(cmdlinebuf.get(), buflen, L"\"%s\" e \"%s\" -o\"%s\" -p%s -y", m_sevenzip.c_str(), crypt.c_str(), targetfolder.c_str(), password.c_str());
    bool bRet = Run7Zip(cmdlinebuf.get(), targetfolder);
    if (!bRet)
    {
        SHCreateDirectory(NULL, targetfolder.c_str());
        bRet = Run7Zip(cmdlinebuf.get(), targetfolder);
    }
    if (bRet)
    {
        // set the file timestamp
        int retry = 5;
        do 
        {
            CAutoFile hFile = CreateFile(orig.c_str(), GENERIC_WRITE|GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
            if (hFile.IsValid())
            {
                bRet = !!SetFileTime(hFile, NULL, NULL, &fd.ft);
            }
            else
                bRet = false;
            if (!bRet)
                Sleep(200);
        } while (!bRet && (retry-- > 0));
    }
    return bRet;
}

std::wstring CFolderSync::GetDecryptedFilename( const std::wstring& filename, const std::wstring& password, bool encryptname ) const
{
    bool bResult = true;
    std::wstring decryptName = filename;
    size_t dotpos = filename.find_last_of('.');
    std::string fname;
    if (dotpos != std::string::npos)
        fname = CUnicodeUtils::StdGetUTF8(filename.substr(0, dotpos));
    else
        fname = CUnicodeUtils::StdGetUTF8(filename);

    if (!encryptname)
    {
        size_t pos = filename.find(L".cryptsync");
        if (pos != std::string::npos)
        {
            return filename.substr(0, pos);
        }
        return filename;
    }

    std::unique_ptr<BYTE[]> strIn(new BYTE[fname.size()*sizeof(WCHAR) + 1]);
    if (!CStringUtils::FromHexString(fname, strIn.get()))
        return filename;

    HCRYPTPROV hProv = NULL;
    // Get handle to user default provider.
    if (CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT|CRYPT_SILENT))
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
                    dwLength = fname.size() + 1024; // 1024 bytes should be enough for padding
                    std::unique_ptr<BYTE[]> buffer(new BYTE[dwLength]);
                    // copy encrypted password to temporary buffer
                    memcpy(buffer.get(), strIn.get(), fname.size());
                    if (!CryptDecrypt(hKey, 0, true, 0, (BYTE *)buffer.get(), &dwLength))
                        bResult = false;
                    CryptDestroyKey(hKey);  // Release provider handle.

                    decryptName = CUnicodeUtils::StdGetUnicode(std::string((char*)buffer.get(), fname.size()/2));
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

std::wstring CFolderSync::GetEncryptedFilename( const std::wstring& filename, const std::wstring& password, bool encryptname ) const
{
    std::wstring encryptFilename = filename;
    if (!encryptname)
    {
        encryptFilename += L".cryptsync";
        return encryptFilename;
    }

    bool bResult = true;

    HCRYPTPROV hProv = NULL;
    // Get handle to user default provider.
    if (CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT|CRYPT_SILENT))
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
                    std::string starname = "*";
                    starname += CUnicodeUtils::StdGetUTF8(filename);

                    dwLength = starname.size();
                    std::unique_ptr<BYTE[]> buffer(new BYTE[dwLength+1024]);
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

bool CFolderSync::Run7Zip( LPWSTR cmdline, const std::wstring& cwd ) const
{
    PROCESS_INFORMATION pi = {0};
    if (CCreateProcessHelper::CreateProcess(m_sevenzip.c_str(), cmdline, cwd.c_str(), &pi, true))
    {
        // wait until the process terminates
        WaitForSingleObject(pi.hProcess, INFINITE);

        DWORD exitcode = 0;
        GetExitCodeProcess(pi.hProcess, &exitcode);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);

        return (exitcode == 0);
    }
    return false;
}
