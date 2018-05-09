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

# linking
The 7zip-Library is linked statically.
For this to work properly, you have to add the library as a reference and then set the library-inputs flag. So, in the VS Solution explorer:
1. right click on "References" in your VS project, select "Add Reference".
2. Select the 7zip library from the list - the library is added to your project.
3. click on the 7zip library in the References folder, hit F4
4. In the properties window, find the option "Use Library dependency inputs" and set this to "true".

Without this setting, the archive handlers are not registered and you'll get errors when trying to use the library.