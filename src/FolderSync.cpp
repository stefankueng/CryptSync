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
#include <cctype>
#include <algorithm>

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


void CFolderSync::Stop()
{
    InterlockedExchange(&m_bRunning, FALSE);
    if (m_hThread)
    {
        WaitForSingleObject(m_hThread, INFINITE);
        m_hThread.CloseHandle();
    }
}

void CFolderSync::SyncFolders( const PairVector& pv, HWND hWnd )
{
    if (m_bRunning)
    {
        Stop();
    }
    CAutoWriteLock locker(m_guard);
    m_pairs = pv;
    m_parentWnd = hWnd;
    unsigned int threadId = 0;
    InterlockedExchange(&m_bRunning, TRUE);
    m_hThread = (HANDLE)_beginthreadex(NULL, 0, SyncFolderThreadEntry, this, 0, &threadId);
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
    {
        CAutoWriteLock locker(m_failureguard);
        m_failures.clear();
    }
    for (auto it = pv.cbegin(); (it != pv.cend()) && m_bRunning; ++it)
    {
        {
            CAutoWriteLock locker(m_guard);
            m_currentPath = *it;
        }
        SyncFolder(*it);
        {
            CAutoWriteLock locker(m_guard);
            m_currentPath = PairTuple();
        }
    }
    if (m_pProgDlg)
    {
        delete m_pProgDlg;
        m_pProgDlg = NULL;
        CoUninitialize();
    }
    PostMessage(m_parentWnd, WM_THREADENDED, 0, 0);
    m_parentWnd = NULL;
    InterlockedExchange(&m_bRunning, FALSE);
}


void CFolderSync::SyncFile( const std::wstring& path )
{
    // check if the path notification comes from a folder that's
    // currently synced in the sync thread
    {
        std::wstring s = std::get<0>(m_currentPath);
        if (!s.empty())
        {
            if ((path.size() > s.size()) &&
                (_wcsicmp(s.c_str(), path.substr(0, s.size()).c_str())==0))
                return;
        }
        s = std::get<1>(m_currentPath);
        if (!s.empty())
        {
            if ((path.size() > s.size()) &&
                (_wcsicmp(s.c_str(), path.substr(0, s.size()).c_str())==0))
                return;
        }
    }

    PairTuple pt;
    CAutoReadLock locker(m_guard);
    for (auto it = m_pairs.cbegin(); it != m_pairs.cend(); ++it)
    {
        std::wstring s = std::get<0>(*it);
        if (path.size() > s.size())
        {
            if ((_wcsicmp(path.substr(0, s.size()).c_str(), s.c_str())==0)&&(path[s.size()] == '\\'))
            {
                pt = *it;
                SyncFile(path, pt);
                continue;
            }
        }
        s = std::get<1>(*it);
        if (path.size() > s.size())
        {
            if ((_wcsicmp(path.substr(0, s.size()).c_str(), s.c_str())==0)&&(path[s.size()] == '\\'))
            {
                pt = *it;
                SyncFile(path, pt);
                continue;
            }
        }
    }
}

void CFolderSync::SyncFile( const std::wstring& path, const PairTuple& pt )
{
    std::wstring orig  = std::get<0>(pt);
    std::wstring crypt = std::get<1>(pt);
    if (orig.empty() || crypt.empty())
        return;

    if ((orig.size() < path.size())&&(_wcsicmp(path.substr(0, orig.size()).c_str(), orig.c_str())==0))
    {
        crypt = crypt + L"\\" + GetEncryptedFilename(path.substr(orig.size()), std::get<2>(pt), std::get<3>(pt));
        orig = path;
    }
    else
    {
        orig = orig + L"\\" + GetDecryptedFilename(path.substr(crypt.size()), std::get<2>(pt), std::get<3>(pt));
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
        (_wcsicmp(orig.c_str(), path.c_str())==0))
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
        if (SHFileOperation(&fop))
        {
            // in case the notification was for a folder that got removed,
            // the GetDecryptedFilename() call above added the .cryptsync extension which
            // folders don't have. Remove that extension and try deleting again.
            crypt = crypt.substr(0, crypt.find_last_of('.'));
            wcscpy_s(delbuf.get(), crypt.size()+2, crypt.c_str());
            delbuf[crypt.size()] = 0;
            delbuf[crypt.size()+1] = 0;
            fop.pFrom = delbuf.get();
            SHFileOperation(&fop);
        }
        return;
    }
    else if ((fdatacrypt.ftLastWriteTime.dwLowDateTime == 0)&&(fdatacrypt.ftLastWriteTime.dwHighDateTime == 0)&&
        (_wcsicmp(crypt.c_str(), path.c_str())==0))
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

    if (fdataorig.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        return;
    if (fdatacrypt.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        return;

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
    auto origFileList  = GetFileList(std::get<0>(pt), std::get<2>(pt), std::get<3>(pt));
    auto cryptFileList = GetFileList(std::get<1>(pt), std::get<2>(pt), std::get<3>(pt));

    {
        // upgrade code: in case users had "encrypt filenames" active but were using version 1.0.0
        // then we have to rename the folders since foldernames weren't encrypted back then
        std::set<std::wstring, ci_less> directories;
        for (auto it = cryptFileList.cbegin(); (it != cryptFileList.cend()) && m_bRunning; ++it)
        {
            std::wstring fenc = GetEncryptedFilename(it->first, std::get<2>(pt), std::get<3>(pt));
            if (_wcsicmp(fenc.c_str(), it->second.filerelpath.c_str())==0)
                break;
            std::wstring srcPath = std::get<1>(pt) + L"\\" + it->second.filerelpath;
            std::wstring destPath = std::get<1>(pt) + L"\\" + fenc;
            std::wstring srcDir = srcPath.substr(0, srcPath.find_last_of('\\'));
            if (!MoveFileEx(srcPath.c_str(), destPath.c_str(), MOVEFILE_COPY_ALLOWED|MOVEFILE_REPLACE_EXISTING))
            {
                std::wstring targetfolder = destPath.substr(0, destPath.find_last_of('\\'));
                SHCreateDirectory(NULL, targetfolder.c_str());
                MoveFileEx(srcPath.c_str(), destPath.c_str(), MOVEFILE_COPY_ALLOWED|MOVEFILE_REPLACE_EXISTING);
            }
            directories.insert(srcDir);
        }
        for (auto srcDir = directories.crbegin(); srcDir != directories.crend(); ++srcDir)
        {
            RemoveDirectory(srcDir->c_str());
        }
    }

    m_progressTotal += (origFileList.size() + cryptFileList.size());

    for (auto it = origFileList.cbegin(); (it != origFileList.cend()) && m_bRunning; ++it)
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
            std::wstring cryptpath = std::get<1>(pt) + L"\\" + GetEncryptedFilename(it->first, std::get<2>(pt), std::get<3>(pt));
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
                CTraceToOutputDebugString::Instance()(_T(__FUNCTION__) _T(": file %s is older than its encrypted partner\n"), it->first.c_str());
                std::wstring cryptpath = std::get<1>(pt) + L"\\" + GetEncryptedFilename(it->first, std::get<2>(pt), std::get<3>(pt));
                std::wstring origpath = std::get<0>(pt) + L"\\" + it->first;
                DecryptFile(origpath, cryptpath, std::get<2>(pt), it->second);
            }
            else if (cmp > 0)
            {
                // encrypted file is older than the original file
                // encrypt the file
                CTraceToOutputDebugString::Instance()(_T(__FUNCTION__) _T(": file %s is newer than its encrypted partner\n"), it->first.c_str());
                std::wstring cryptpath = std::get<1>(pt) + L"\\" + GetEncryptedFilename(it->first, std::get<2>(pt), std::get<3>(pt));
                std::wstring origpath = std::get<0>(pt) + L"\\" + it->first;
                EncryptFile(origpath, cryptpath, std::get<2>(pt), it->second);
            }
            else if (cmp == 0)
            {
                // files are identical (have the same last-write-time):
                // nothing to do.
            }
        }
    }
    // now go through the encrypted file list and if there's a file that's not in the original file list,
    // decrypt it
    if (!std::get<4>(pt) || origFileList.empty())
    {
        for (auto it = cryptFileList.cbegin(); (it != cryptFileList.cend()) && m_bRunning; ++it)
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
                std::wstring cryptpath = std::get<1>(pt) + L"\\" + it->second.filerelpath;
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
}

std::map<std::wstring,FileData, ci_less> CFolderSync::GetFileList( const std::wstring& path, const std::wstring& password, bool encnames ) const
{
    CDirFileEnum enumerator(path);

    std::map<std::wstring,FileData, ci_less> filelist;
    std::wstring filepath;
    bool isDir = false;
    while (enumerator.NextFile(filepath, &isDir, true))
    {
        if (isDir)
            continue;

        if (!m_bRunning)
            break;

        FileData fd;

        fd.ft = enumerator.GetLastWriteTime();
        if ((fd.ft.dwLowDateTime == 0) && (fd.ft.dwHighDateTime == 0))
            fd.ft = enumerator.GetCreateTime();

        std::wstring relpath = filepath.substr(path.size()+1);
        fd.filerelpath = relpath;

        std::wstring decryptedRelPath = GetDecryptedFilename(relpath, password, encnames);
        fd.filenameEncrypted = (_wcsicmp(decryptedRelPath.c_str(), fd.filerelpath.c_str())!=0);
        if (fd.filenameEncrypted)
        {
            relpath = decryptedRelPath;
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
    if ((!cryptname.empty()) && (cryptname[0] == '-'))
        cryptname = L".\\" + cryptname;

    // 7zip 9.30 has an option "-stl" which sets the timestamp of the archive
    // to the most recent one of the compressed files
    // add this flag as soon as 9.30 is stable and officially released.
    swprintf_s(cmdlinebuf.get(), buflen, L"\"%s\" a -t7z \"%s\" \"%s\" -mx9 -p\"%s\" -mhe=on -w", m_sevenzip.c_str(), cryptname.c_str(), orig.c_str(), password.c_str());
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
        CAutoWriteLock locker(m_failureguard);
        m_failures.erase(orig);
    }
    else
    {
        CAutoWriteLock locker(m_failureguard);
        m_failures[orig] = Encrypt;
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
    swprintf_s(cmdlinebuf.get(), buflen, L"\"%s\" e \"%s\" -o\"%s\" -p\"%s\" -y", m_sevenzip.c_str(), crypt.c_str(), targetfolder.c_str(), password.c_str());
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
        CAutoWriteLock locker(m_failureguard);
        m_failures.erase(orig);
    }
    else
    {
        CAutoWriteLock locker(m_failureguard);
        m_failures[orig] = Decrypt;
    }
    return bRet;
}

std::wstring CFolderSync::GetDecryptedFilename( const std::wstring& filename, const std::wstring& password, bool encryptname ) const
{
    bool bResult = true;
    std::wstring decryptName = filename;
    size_t dotpos = filename.find_last_of('.');
    std::wstring fname;
    if (dotpos != std::string::npos)
        fname = filename.substr(0, dotpos);
    else
        fname = filename;

    if (!encryptname)
    {
        std::wstring f = filename;
        std::transform(f.begin(), f.end(), f.begin(), std::tolower);
        size_t pos = f.find(L".cryptsync");
        if ((pos != std::string::npos) && (pos == (filename.size() - 10)))
        {
            return filename.substr(0, pos);
        }
        return filename;
    }

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
                    std::vector<std::wstring> names;
                    std::vector<std::wstring> decryptnames;
                    stringtok(names, fname, true, L"\\/");
                    for (auto it = names.cbegin(); it != names.cend(); ++it)
                    {
                        std::string name = CUnicodeUtils::StdGetUTF8(*it);
                        dwLength = name.size() + 1024; // 1024 bytes should be enough for padding
                        std::unique_ptr<BYTE[]> buffer(new BYTE[dwLength]);

                        std::unique_ptr<BYTE[]> strIn(new BYTE[name.size()*sizeof(WCHAR) + 1]);
                        if (CStringUtils::FromHexString(name, strIn.get()))
                        {
                            // copy encrypted password to temporary buffer
                            memcpy(buffer.get(), strIn.get(), name.size());
                            if (!CryptDecrypt(hKey, 0, true, 0, (BYTE *)buffer.get(), &dwLength))
                                bResult = false;
                            decryptName = CUnicodeUtils::StdGetUnicode(std::string((char*)buffer.get(), name.size()/2));
                        }
                        else
                            decryptName = *it;
                        if (decryptName.empty() || (decryptName[0] != '*'))
                        {
                            if ((dotpos != std::string::npos)&&((it+1)==names.cend()))
                            {
                                std::wstring s = *it;
                                s += filename.substr(dotpos);
                                decryptnames.push_back(s);
                            }
                            else
                                decryptnames.push_back(*it);
                        }
                        else
                            decryptnames.push_back(decryptName.substr(1));    // cut off the starting '*'
                    }
                    CryptDestroyKey(hKey);  // Release provider handle.
                    decryptName.clear();
                    for (auto it = decryptnames.cbegin(); it != decryptnames.cend(); ++it)
                    {
                        if (!decryptName.empty())
                            decryptName += L"\\";
                        decryptName += *it;
                    }
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
    else
        DebugBreak();

    return decryptName;
}

std::wstring CFolderSync::GetEncryptedFilename( const std::wstring& filename, const std::wstring& password, bool encryptname ) const
{
    std::wstring encryptFilename = filename;
    if (!encryptname)
    {
        std::wstring f = filename;
        std::transform(f.begin(), f.end(), f.begin(), std::tolower);
        size_t pos = f.find(L".cryptsync");
        if ((pos == std::string::npos) || (pos != (filename.size() - 10)))
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
                    std::vector<std::wstring> names;
                    std::vector<std::wstring> encryptnames;
                    stringtok(names, filename, true, L"\\/");
                    for (auto it = names.cbegin(); it != names.cend(); ++it)
                    {
                        // Determine number of bytes to encrypt at a time.
                        std::string starname = "*";
                        starname += CUnicodeUtils::StdGetUTF8(*it);

                        dwLength = starname.size();
                        std::unique_ptr<BYTE[]> buffer(new BYTE[dwLength+1024]);
                        memcpy(buffer.get(), starname.c_str(), dwLength);
                        // Encrypt data
                        if (CryptEncrypt(hKey, 0, true, 0, buffer.get(), &dwLength, dwLength+1024))
                        {
                            encryptFilename = CStringUtils::ToHexWString(buffer.get(), dwLength);
                            encryptnames.push_back(encryptFilename);
                        }
                        else
                        {
                            encryptnames.push_back(*it);
                            bResult = false;
                        }
                    }
                    encryptFilename.clear();
                    for (auto it = encryptnames.cbegin(); it != encryptnames.cend(); ++it)
                    {
                        if (!encryptFilename.empty())
                            encryptFilename += L"\\";
                        encryptFilename += *it;
                    }
                    encryptFilename += L".cryptsync";

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
    else
        DebugBreak();
    if (bResult)
        return encryptFilename;
    return filename;
}

bool CFolderSync::Run7Zip( LPWSTR cmdline, const std::wstring& cwd ) const
{
    PROCESS_INFORMATION pi = {0};
    if (CCreateProcessHelper::CreateProcess(m_sevenzip.c_str(), cmdline, cwd.c_str(), &pi, true, BELOW_NORMAL_PRIORITY_CLASS|CREATE_UNICODE_ENVIRONMENT))
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

std::map<std::wstring, SyncOp> CFolderSync::GetFailures()
{
    CAutoReadLock locker(m_failureguard);
    return m_failures;
}

size_t CFolderSync::GetFailureCount()
{
    CAutoReadLock locker(m_failureguard);
    return m_failures.size();
}
