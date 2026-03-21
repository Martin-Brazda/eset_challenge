#pragma once
#include <string>

/**
 * @brief Application configuration loaded from .env or environment.
 */
struct Config {
    size_t num_threads = 4;
    size_t queue_max_size = 0;
    size_t buffer_size = 8192;
    size_t batch_size = 100;
    bool logging_enabled = false;
    std::string logging_level = "INFO";
    std::string logging_output = "STDOUT";

    void load_env(const std::string& filepath);
};