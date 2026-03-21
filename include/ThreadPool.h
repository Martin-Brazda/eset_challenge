#pragma once
#include <vector>
#include <thread>
#include <queue>
#include <functional>
#include <future>
#include <mutex>
#include <condition_variable>
#include <optional>
#include "ThreadQueue.h"

/**
 * @brief Managed pool of worker threads.
 * 
 * Uses a ThreadQueue to dispatch tasks to a fixed number of background threads.
 */
class ThreadPool {
public:
    /**
     * @brief Initializes the pool and starts worker threads.
     * @param num_threads Number of worker threads to spawn.
     * @param queue_max_size Optional limit on task queue size (0 = unbounded).
     */
    explicit ThreadPool(size_t num_threads = std::thread::hardware_concurrency(), size_t queue_max_size = 0) 
        : queue_(queue_max_size) {
        for (size_t i = 0; i < num_threads; ++i) {
            workers_.emplace_back([this]() {
                while (true) {
                    auto task = queue_.pop();
                    if (!task) break;
                    (*task)();
                }
            });
        }
    }

    /** @brief Destructor shuts down the pool and joins all threads. */
    ~ThreadPool() {
        stop();
    }

    /** @brief Enqueues a new task for execution. */
    void submit(std::function<void()> task) {
        queue_.push(std::move(task));
    }

    /** @brief Stops accepting new tasks and blocks until queue is empty. */
    void stop() {
        queue_.done();
        for (auto& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }

private:
    ThreadQueue<std::function<void()>> queue_;
    std::vector<std::thread> workers_;
};
