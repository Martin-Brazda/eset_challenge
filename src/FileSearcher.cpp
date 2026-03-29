#include "FileSearcher.h"
#include <fstream>
#include <iostream>
#include <vector>
#include <cstring>
#include <algorithm>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#define SMALL_FILE_THRESHOLD 1024
#define CHUNKING_THRESHOLD 64*1024*1024

FileSearcher::FileSearcher(const SearchEngine& engine, const Config& config) : engine_(engine), config_(config) {}

SearchResult FileSearcher::searchSmallFile(const std::string& filepath) const {
    #ifdef _WIN32
            std::ifstream file(std::filesystem::path(filepath), std::ios::binary);
        #else
            std::ifstream file(filepath, std::ios::binary);
        #endif
        if (!file.is_open()) {
            if (config_.logging_enabled) {
                std::cerr << "Error opening file: " << filepath << std::endl;
            }
            return {-1, "", ""};
        }

        std::vector<char> buffer(config_.buffer_size + getNeedle().size());
        int64_t current_offset = 0;
        size_t overlap = 0;

        while (file) {
            file.read(buffer.data() + overlap, buffer.size() - overlap);
            std::streamsize bytes_read = file.gcount();
            if (bytes_read == 0) break;

            size_t total = overlap + static_cast<size_t>(bytes_read);

            int64_t pos = engine_.searchBuffer(buffer.data(), total);
                                
            if (pos != -1) {
                int64_t start_idx = std::max(0L, pos - 3);
                int64_t end_idx = std::min(static_cast<int64_t>(total), pos + static_cast<int64_t>(getNeedle().size()) + 3);
                std::string prefix(buffer.begin() + start_idx, buffer.begin() + pos);
                std::string suffix(buffer.begin() + pos + getNeedle().size(), buffer.begin() + end_idx);
                return {current_offset - static_cast<int64_t>(overlap) + pos, prefix, suffix};
            }

            overlap = std::min(getNeedle().size() - 1, total);
            std::memmove(buffer.data(), buffer.data() + total - overlap, overlap);

            current_offset += bytes_read; 
        }
        return {-1, "", ""};
}

SearchResult FileSearcher::searchMmapChunked(int fd, size_t file_size, const std::string& filepath) const {
    size_t chunk_size = CHUNKING_THRESHOLD;
    size_t overlap = getNeedle().empty() ? 0 : getNeedle().size() - 1;
    long page_size = sysconf(_SC_PAGE_SIZE);
    
    for (size_t logical_offset = 0; logical_offset < file_size; ) {
        size_t aligned_offset = (logical_offset / page_size) * page_size;
        size_t map_offset_diff = logical_offset - aligned_offset;
        
        size_t map_size = std::min(chunk_size, file_size - aligned_offset);
        
        char* mapped_addr = (char*)mmap(NULL, map_size, PROT_READ, MAP_PRIVATE, fd, aligned_offset);
        if (mapped_addr == MAP_FAILED) {
            if (config_.logging_enabled) {
                std::cerr << "Error mapping file chunk: " << filepath << " at offset " << aligned_offset << std::endl;
            }
            close(fd);
            return {-1, "", ""};
        }
        
        char* search_start = mapped_addr + map_offset_diff;
        size_t search_len = map_size - map_offset_diff;
        
        int64_t pos = engine_.searchBuffer(search_start, search_len);
        if (pos != -1) {
            int64_t global_pos = logical_offset + pos;
            munmap(mapped_addr, map_size);
            
            int64_t start_idx = std::max(0L, global_pos - 3);
            int64_t end_idx = std::min((int64_t)file_size, global_pos + (int64_t)getNeedle().size() + 3);
            size_t context_len = end_idx - start_idx;
            
            std::vector<char> context_buf(context_len);
            pread(fd, context_buf.data(), context_len, start_idx);
            
            close(fd);
            
            int64_t relative_pos = global_pos - start_idx;
            std::string prefix(context_buf.begin(), context_buf.begin() + relative_pos);
            std::string suffix(context_buf.begin() + relative_pos + getNeedle().size(), context_buf.end());
            
            return {global_pos, prefix, suffix};
        }
        
        munmap(mapped_addr, map_size);
        
        if (map_size == file_size - aligned_offset) {
            break;
        }
        logical_offset += search_len - overlap;
    }
    
    close(fd);
    return {-1, "", ""};
}

SearchResult FileSearcher::search(const std::string& filepath) const {
    int fd = open(filepath.c_str(), O_RDONLY);
    if (fd == -1) return {-1, "", ""};
    
    struct stat st;
    if (fstat(fd, &st) == -1) {
        close(fd);
        return {-1, "", ""};
    }

    size_t file_size = st.st_size;
    if (file_size == 0) {
        close(fd);
        return {-1, "", ""};
    }

    if (file_size < SMALL_FILE_THRESHOLD) {
        close(fd);
        return searchSmallFile(filepath);
    } else if (file_size >= SMALL_FILE_THRESHOLD) {
        return searchMmapChunked(fd, file_size, filepath);
    } else {
        close(fd);
    }

    return {-1, "", ""}; 
}
