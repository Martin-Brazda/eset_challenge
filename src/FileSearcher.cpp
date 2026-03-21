#include "FileSearcher.h"
#include <fstream>
#include <iostream>
#include <vector>
#include <cstring>
#include <algorithm>

FileSearcher::FileSearcher(const SearchEngine& engine, const Config& config) : engine_(engine), config_(config) {}

SearchResult FileSearcher::search(const std::string& filepath) const {
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        if (config_.logging_enabled) {
            std::cerr << "Error opening file: " << filepath << std::endl;
        }
        return {-1, "", ""};
    }

    std::vector<char> buffer(config_.buffer_size + getNeedle().size());
    long current_offset = 0;
    size_t overlap = 0;

    while (file) {
        file.read(buffer.data() + overlap, buffer.size() - overlap);
        std::streamsize bytes_read = file.gcount();
        if (bytes_read == 0) break;

        size_t total = overlap + bytes_read;

        long pos = engine_.searchBuffer(buffer.data(), total);
                              
        if (pos != -1) {
            long start_idx = std::max(0L, pos - 3);
            long end_idx = std::min(static_cast<long>(total), pos + static_cast<long>(getNeedle().size()) + 3);
            std::string prefix(buffer.begin() + start_idx, buffer.begin() + pos);
            std::string suffix(buffer.begin() + pos + getNeedle().size(), buffer.begin() + end_idx);
            return {current_offset - static_cast<long>(overlap) + pos, prefix, suffix};
        }

        overlap = std::min(getNeedle().size() - 1, total);
        std::memmove(buffer.data(), buffer.data() + total - overlap, overlap);

        current_offset += bytes_read; 
    }

    return {-1, "", ""}; 
}
