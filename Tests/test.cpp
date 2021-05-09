#include "stdafx.h"

#include "../src/FolderSync.h"

#pragma warning(disable: 4566) // character represented by ... cannot be represented in the current code page

TEST(NameEncryption, decrypt_old_encryption)
{
    auto decryptedFilename = CFolderSync::GetDecryptedFilename(L"77fd5c174b90a159d0e7b9fa2e.7z", L"password", true, false, true, false);
    EXPECT_EQ(decryptedFilename, L"filename.txt");
}

TEST(NameEncryption, encrypt_old_encryption)
{
    auto decryptedFilename = CFolderSync::GetEncryptedFilename(L"filename.txt", L"password", true, false, true, false);
    EXPECT_EQ(decryptedFilename, L"77fd5c174b90a159d0e7b9fa2e.7z");
}

TEST(NameEncryption, decrypt_new_encryption)
{
    auto decryptedFilename = CFolderSync::GetDecryptedFilename(L"板浜慴殐樕槐湻槺䀮.7z", L"password", true, true, true, false);
    EXPECT_EQ(decryptedFilename, L"filename.txt");
}

TEST(NameEncryption, encrypt_new_encryption)
{
    auto encryptedFilename = CFolderSync::GetEncryptedFilename(L"filename.txt", L"password", true, true, true, false);
    EXPECT_EQ(encryptedFilename, L"板浜慴殐樕槐湻槺䀮.7z");
}