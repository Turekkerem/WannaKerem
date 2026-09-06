#include "common.hpp"

#include <windows.h>
#include <shlobj.h>
#include <combaseapi.h>
#include <openssl/sha.h>
#include <openssl/rand.h>
#include <vector>
#include <string>
#include <cstring>
#include <stdexcept>
#include <algorithm>
#include <cstdint>
#include <cctype>
#include <filesystem>
#include <fstream>

static bool WriteFileContent(const std::wstring& path, const void* data, size_t size) {
    HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0;
    bool ok = WriteFile(h, data, static_cast<DWORD>(size), &written, nullptr) && written == size;
    CloseHandle(h);
    return ok;
}

static bool ReadFileContent(const std::wstring& path, std::vector<uint8_t>& buffer) {
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER size;
    if (!GetFileSizeEx(h, &size)) { CloseHandle(h); return false; }
    buffer.resize(static_cast<size_t>(size.QuadPart));
    DWORD read = 0;
    bool ok = ReadFile(h, buffer.data(), static_cast<DWORD>(buffer.size()), &read, nullptr) && read == buffer.size();
    CloseHandle(h);
    return ok;
}


bool FileExists(const std::wstring& path) {
    DWORD attr = GetFileAttributesW(path.c_str());
    return (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY));
}

bool HasReadWriteAccess(const std::wstring& filePath) {
    DWORD attr = GetFileAttributesW(filePath.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES || (attr & FILE_ATTRIBUTE_READONLY)) return false;
    HANDLE h = CreateFileW(filePath.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ,
                           nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    CloseHandle(h);
    return true;
}

bool SecureDeleteFile(const std::wstring& filePath) {
    if (!FileExists(filePath)) return false;
    HANDLE h = CreateFileW(filePath.c_str(), GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
                           FILE_FLAG_WRITE_THROUGH, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;

    LARGE_INTEGER size;
    if (!GetFileSizeEx(h, &size) || size.QuadPart <= 0) { CloseHandle(h); return false; }

    constexpr DWORD BUF_SIZE = 4096;
    std::vector<BYTE> buffer(BUF_SIZE);

    for (int pass = 0; pass < 3; ++pass) {
        LARGE_INTEGER zero = {0};
        SetFilePointerEx(h, zero, nullptr, FILE_BEGIN);
        LONGLONG left = size.QuadPart;
        while (left > 0) {
            DWORD chunk = static_cast<DWORD>(std::min<LONGLONG>(left, BUF_SIZE));
            if (pass == 0) FillMemory(buffer.data(), chunk, 0x00);
            else if (pass == 1) FillMemory(buffer.data(), chunk, 0xFF);
            else RAND_bytes(buffer.data(), chunk);
            DWORD written = 0;
            if (!WriteFile(h, buffer.data(), chunk, &written, nullptr) || written != chunk) {
                CloseHandle(h); return false;
            }
            left -= written;
        }
        FlushFileBuffers(h);
    }
    SecureZeroMemory(buffer.data(), buffer.size());
    CloseHandle(h);
    return DeleteFileW(filePath.c_str()) != 0;
}

std::wstring GetUserDesktopPath() {
    PWSTR pszPath;
    HRESULT hr = SHGetKnownFolderPath(FOLDERID_Desktop, 0, NULL, &pszPath);
    if (FAILED(hr))
        throw std::runtime_error("Failed to get desktop path");
    std::wstring path(pszPath);
    CoTaskMemFree(pszPath);
    return path;
}

std::wstring GetUniqueFilePath(const std::wstring& basePath) {
    if (!FileExists(basePath)) return basePath;

    std::wstring dir = basePath.substr(0, basePath.find_last_of(L"\\/") + 1);
    std::wstring name = basePath.substr(basePath.find_last_of(L"\\/") + 1);
    size_t dot = name.find_last_of(L'.');
    std::wstring stem = (dot == std::wstring::npos) ? name : name.substr(0, dot);
    std::wstring ext  = (dot == std::wstring::npos) ? L"" : name.substr(dot);

    for (int i = 2; ; ++i) {
        std::wstring candidate = dir + stem + L"_" + std::to_wstring(i) + ext;
        if (!FileExists(candidate)) return candidate;
    }
}


void SaveKeyHash(const SecureBuffer& key, const std::wstring& hashPath) {
    uint8_t hash[SHA256_DIGEST_LENGTH];
    SHA256_CTX ctx;
    SHA256_Init(&ctx);
    SHA256_Update(&ctx, key.data(), key.size());
    SHA256_Final(hash, &ctx);

    if (!WriteFileContent(hashPath, hash, SHA256_DIGEST_LENGTH))
        throw std::runtime_error("Cannot create hash file");
}
void DebugWriteHex(const std::string& hex, const std::wstring& path) {
    std::ofstream file{std::filesystem::path(path)};   // klamry!
    if (file.is_open()) {
        file << hex;
        file.close();
    }
}

static std::vector<uint8_t> ComputeSha256(const std::vector<uint8_t>& data) {
    std::vector<uint8_t> hash(SHA256_DIGEST_LENGTH);
    SHA256_CTX ctx;
    SHA256_Init(&ctx);
    SHA256_Update(&ctx, data.data(), data.size());
    SHA256_Final(hash.data(), &ctx);
    return hash;
}

bool CheckKeyValidity(const std::string& keyHex, const std::wstring& hashPath) {
    if (keyHex.length() != AES_256_KEY_SIZE * 2) return false;

    std::vector<uint8_t> keyBytes(AES_256_KEY_SIZE);
    for (size_t i = 0; i < AES_256_KEY_SIZE; ++i) {
        char byteStr[3] = { keyHex[i*2], keyHex[i*2+1], '\0' };
        char* endptr = nullptr;
        long val = strtol(byteStr, &endptr, 16);
        if (endptr == byteStr || *endptr != '\0') return false;
        keyBytes[i] = static_cast<uint8_t>(val);
    }

    std::vector<uint8_t> computedHash = ComputeSha256(keyBytes);
    std::vector<uint8_t> storedHash;
    if (!ReadFileContent(hashPath, storedHash)) return false;
    if (storedHash.size() != SHA256_DIGEST_LENGTH) return false;
    return memcmp(computedHash.data(), storedHash.data(), SHA256_DIGEST_LENGTH) == 0;
}

// Wersja do użycia, gdy mamy już SecureBuffer
bool CheckKeyValidityFromBuffer(const SecureBuffer& key, const std::wstring& hashPath) {
    if (key.size() != AES_256_KEY_SIZE) return false;
    std::vector<uint8_t> keyBytes(key.data(), key.data() + key.size());
    std::vector<uint8_t> computedHash = ComputeSha256(keyBytes);
    std::vector<uint8_t> storedHash;
    if (!ReadFileContent(hashPath, storedHash) || storedHash.size() != SHA256_DIGEST_LENGTH)
        return false;
    return memcmp(computedHash.data(), storedHash.data(), SHA256_DIGEST_LENGTH) == 0;
}

// Stałe ścieżki do plików klucza
std::wstring GetMasterKeyHashPath() { return GetUserDesktopPath() + L"\\masterkey.sha256"; }
std::wstring GetMasterKeyEncPath()  { return GetUserDesktopPath() + L"\\masterkey.enc"; }
// ============================================================================
// Konwersje
// ============================================================================
SecureBuffer MasterKeyFromHexWString(const std::wstring& hex) {
    if (hex.length() != AES_256_KEY_SIZE * 2)
        throw std::runtime_error("Invalid hex length – expected 64 characters");

    SecureBuffer key(AES_256_KEY_SIZE);
    for (size_t i = 0; i < AES_256_KEY_SIZE; ++i) {
        wchar_t high = hex[i * 2];
        wchar_t low  = hex[i * 2 + 1];

        auto hexVal = [](wchar_t c) -> int {
            if (c >= L'0' && c <= L'9') return c - L'0';
            if (c >= L'A' && c <= L'F') return c - L'A' + 10;
            if (c >= L'a' && c <= L'f') return c - L'a' + 10;
            return -1;
        };

        int h = hexVal(high);
        int l = hexVal(low);
        if (h == -1 || l == -1)
            throw std::runtime_error("Invalid hex character");

        key.data()[i] = static_cast<uint8_t>((h << 4) | l);
    }
    return key;
}

std::string WStringToString(const std::wstring& wstr) {
    std::string str;
    str.reserve(wstr.size());
    for (wchar_t c : wstr) {
        if (c > 127) throw std::runtime_error("Non-ASCII character in key");
        str.push_back(static_cast<char>(c));
    }
    return str;
}
std::wstring GetMarkerFilePath() {
    wchar_t path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_COMMON_APPDATA, NULL, 0, path))) {
        return std::wstring(path) + L"\\marker.dat";
    }
    return L"C:\\marker.dat";
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