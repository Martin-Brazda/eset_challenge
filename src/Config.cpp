#include "Config.h"
#include <string>
#include <fstream>
#include <iostream>

void Config::load_env(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Error opening file: " << filepath << std::endl;
        return;
    }
    
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        
        std::string prefix = "export ";
        if (line.find(prefix) == 0) {
            line = line.substr(prefix.length());
        }

        size_t eq_pos = line.find('=');
        if (eq_pos == std::string::npos) continue;
        
        std::string key = line.substr(0, eq_pos);
        std::string value = line.substr(eq_pos + 1);
        
        if (key == "NUM_THREADS") {
            num_threads = std::stoi(value);
        } else if (key == "QUEUE_MAX_SIZE") {
            queue_max_size = std::stoi(value);
        } else if (key == "BUFFER_SIZE") {
            buffer_size = std::stoi(value);
        } else if (key == "BATCH_SIZE") {
            batch_size = std::stoi(value);
        } else if (key == "LOGGING_ENABLED") {
            logging_enabled = std::stoi(value);
        } else if (key == "LOGGING_LEVEL") {
            logging_level = value;
        } else if (key == "LOGGING_OUTPUT") {
            logging_output = value;
        }
    }
}
