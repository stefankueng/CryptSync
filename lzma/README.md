# 7zip C++ wrapper

This is a simple to use wrapper for the 7zip library.

# How to use

- list the contents of an archive file:
 ```C++
    auto progressFunc = [](UInt64 pos, UInt64 total, const std::wstring& text)
    {
        cout << pos << L" " << total << L" file: " << text;
        // to cancel, return E_ABORT
        return S_OK;
    };

    C7Zip zipper;
    zipper.SetArchivePath(L"D:\\testp.7z");
    zipper.SetPassword(L"test");
    zipper.SetCallback(progressFunc);

    std::vector<ArchiveFile> filelist;
    zipper.ListFiles(filelist);
```

- Compress a folder:
 ```C++
    C7Zip compressor;
    compressor.SetPassword(L"test");
    compressor.SetArchivePath(L"D:\\archive.7z");
    compressor.SetCompressionFormat(CompressionFormat::SevenZip, 9);
    //compressor.SetCallback(progressFunc);
    compressor.AddPath(L"D:\\datafiles");
```

- Unpack an archive:
 ```C++
    C7Zip extractor;
    extractor.SetPassword(L"test");
    extractor.SetArchivePath(L"D:\\archive.7z");
    extractor.SetCallback(progressFunc);
    extractor.Extract(L"D:\\extract_out");
```
