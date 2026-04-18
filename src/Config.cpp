#include "Config.h"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <string>

namespace {

void trim_inplace(std::string& s) {
    auto not_space = [](unsigned char c) { return !std::isspace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), not_space));
    s.erase(std::find_if(s.rbegin(), s.rend(), not_space).base(), s.end());
}

size_t parse_size(std::string value) {
    trim_inplace(value);
    if (value.empty()) {
        return 0;
    }

    std::string upper = value;
    std::transform(upper.begin(), upper.end(), upper.begin(), [](unsigned char c) {
        return static_cast<char>(std::toupper(c));
    });

    const size_t gb_pos = upper.find("GB");
    const size_t mb_pos = upper.find("MB");
    const size_t kb_pos = upper.find("KB");

    try {
        if (gb_pos != std::string::npos) {
            std::string num = value.substr(0, gb_pos);
            trim_inplace(num);
            return static_cast<size_t>(std::stoull(num)) * 1024ULL * 1024ULL * 1024ULL;
        }
        if (mb_pos != std::string::npos) {
            std::string num = value.substr(0, mb_pos);
            trim_inplace(num);
            return static_cast<size_t>(std::stoull(num)) * 1024ULL * 1024ULL;
        }
        if (kb_pos != std::string::npos) {
            std::string num = value.substr(0, kb_pos);
            trim_inplace(num);
            return static_cast<size_t>(std::stoull(num)) * 1024ULL;
        }
        return static_cast<size_t>(std::stoull(value));
    } catch (const std::invalid_argument& e) {
        std::cerr << "Invalid size value \"" << value << "\": " << e.what() << std::endl;
        return 0;
    } catch (const std::out_of_range& e) {
        std::cerr << "Size value out of range \"" << value << "\": " << e.what() << std::endl;
        return 0;
    }
}

}  // namespace
 
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
        trim_inplace(key);
        trim_inplace(value);

        if (key == "NUM_THREADS") {
            try {
                num_threads = std::stoi(value);
            } catch (const std::invalid_argument& e) {
                std::cerr << "Invalid argument: " << e.what() << std::endl;
                return;
            } catch (const std::out_of_range& e) {
                std::cerr << "Out of range: " << e.what() << std::endl;
                return;
            }
        } else if (key == "QUEUE_MAX_SIZE") {
            try {
                queue_max_size = std::stoi(value);
            } catch (const std::invalid_argument& e) {
                std::cerr << "Invalid argument: " << e.what() << std::endl;
                return;
            } catch (const std::out_of_range& e) {
                std::cerr << "Out of range: " << e.what() << std::endl;
                return;
            }
        } else if (key == "BATCH_SIZE") {
            try {
                batch_size = std::stoi(value);
            } catch (const std::invalid_argument& e) {
                std::cerr << "Invalid argument: " << e.what() << std::endl;
                return;
            } catch (const std::out_of_range& e) {
                std::cerr << "Out of range: " << e.what() << std::endl;
                return;
            }
        } else if (key == "LOGGING_ENABLED") {
            std::string v = value;
            std::transform(v.begin(), v.end(), v.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            logging_enabled = (v == "1" || v == "true" || v == "yes" || v == "on");
        } else if (key == "SMALL_FILE_THRESHOLD") {
            small_file_threshold = parse_size(value);
        } else if (key == "CHUNKING_THRESHOLD") {
            chunking_threshold = parse_size(value);
        } else if (key == "MULTIFIND") {
            std::string v = value;
            std::transform(v.begin(), v.end(), v.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            multifind = (v == "1" || v == "true" || v == "yes" || v == "on");
        }
    }
}
