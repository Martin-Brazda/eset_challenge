#pragma once
#include <queue>
#include <mutex>
#include <condition_variable>
#include <optional>

/**
 * @brief Thread-safe blocking queue for cross-thread task distribution.
 * 
 * Supports bounding, condition variables for efficient waiting, and 
 * a "done" signal for graceful worker shutdown.
 */
template <typename T>
class ThreadQueue {
public:
    explicit ThreadQueue(size_t max_size = 0) : max_size_(max_size), done_(false) {}
    
    void push(T item) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (max_size_ > 0) {
            cv_push_.wait(lock, [this]() { return queue_.size() < max_size_ || done_; });
        }
        if (done_) {
            return;
        }
        queue_.push(std::move(item));
        cv_pop_.notify_one();
    }

    std::optional<T> pop() {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_pop_.wait(lock, [this]() { return !queue_.empty() || done_; });
        
        if (queue_.empty() && done_) {
            return std::nullopt;
        }
        
        T item = std::move(queue_.front());
        queue_.pop();
        
        if (max_size_ > 0) {
            cv_push_.notify_one();
        }
        
        return item;
    }

    void done() {
        std::unique_lock<std::mutex> lock(mutex_);
        done_ = true;
        cv_pop_.notify_all();
        cv_push_.notify_all();
    }

private:
    std::queue<T> queue_;
    std::mutex mutex_;
    std::condition_variable cv_pop_;
    std::condition_variable cv_push_;
    size_t max_size_;
    bool done_;
};
