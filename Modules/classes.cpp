#include "common.hpp"

#include <windows.h>           // VirtualLock, VirtualUnlock, SecureZeroMemory, GetFileAttributesW itp.
#include <vector>              // std::vector
#include <thread>              // std::thread
#include <mutex>               // std::mutex
#include <condition_variable>  // std::condition_variable
#include <queue>               // std::queue
#include <functional>          // std::function
#include <atomic>              // (jeśli ThreadPool używa atomic, ale tu niekoniecznie)
#include <cstring>             // (opcjonalnie) dla memcpy, memset


// ============================================================================
// Klasa SecureBuffer (zabezpieczona pamięć)
// ============================================================================
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


