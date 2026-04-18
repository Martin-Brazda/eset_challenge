#pragma once
#include <iostream>
#include <thread>
#include <atomic>
#include <string>
#include <chrono>

/**
 * @brief Simple threaded progress animation.
 * 
 * Runs a spinner animation in a background thread to indicate that 
 * work is being done. Prints to stderr so it doesn't interfere with main output.
 */
class ProgressAnimation {
public:
    ProgressAnimation() : running_(false) {}

    ~ProgressAnimation() {
        stop();
    }

    void start(const std::string& message = "Searching ") {
        if (running_) return;
        
        running_ = true;
        message_ = message;
        
        worker_ = std::thread([this]() {
            const char spinner[] = {'|', '/', '-', '\\'};
            int i = 0;
            
            while (running_) {
                std::cerr << "\r\033[K" << message_ << spinner[i % 4] << std::flush;
                i++;
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        });
    }

    void stop() {
        if (!running_) return;
        
        running_ = false;
        if (worker_.joinable()) {
            worker_.join();
        }
        
        std::cerr << "\r\033[K" << std::flush;
    }

private:
    std::atomic<bool> running_;
    std::string message_;
    std::thread worker_;
};
