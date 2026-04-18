#pragma once
#include <string>

/**
 * @brief Application configuration loaded from .env or environment.
 */
struct Config {
    size_t num_threads = 4;
    size_t queue_max_size = 0;
    bool logging_enabled = false;
    bool multifind = true;
    /// Files this size or smaller use the single-read path (see FileSearcher).
    size_t small_file_threshold = 64 * 1024;
    /// Max bytes per mmap window when scanning larger files.
    size_t chunking_threshold = 64 * 1024 * 1024;
    size_t batch_size = 100;

    void load_env(const std::string& filepath);
};