#include <FileSearcher.h>
#include <cerrno>
#include <fstream>
#include <iostream>
#include <vector>
#include <cstring>
#include <algorithm>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

namespace {

bool read_full_pread(int fd, char* buf, size_t count, int64_t offset) {
    size_t total = 0;
    while (total < count) {
        const auto off = static_cast<off_t>(offset + static_cast<int64_t>(total));
        ssize_t n = ::pread(fd, buf + total, count - total, off);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return false;
        }
        if (n == 0) {
            return false;
        }
        total += static_cast<size_t>(n);
    }
    return true;
}

SearchResult make_from_buffer(const char* buf, size_t buf_len, int64_t local_pos, int64_t global_pos, const std::string& needle) {
    int64_t start_idx = std::max<int64_t>(0, local_pos - 3);
    int64_t end_idx = std::min(static_cast<int64_t>(buf_len), local_pos + static_cast<int64_t>(needle.size()) + 3);
    return {static_cast<long>(global_pos),
            std::string(buf + start_idx, buf + local_pos),
            std::string(buf + local_pos + static_cast<int64_t>(needle.size()), buf + end_idx)};
}

}  // namespace

FileSearcher::FileSearcher(const SearchEngine& engine, const Config& config)
    : engine_(engine), config_(config) {}

std::vector<SearchResult> FileSearcher::searchSmallFile(int fd, size_t file_size,
                                                        const std::string& filepath) const {
    std::vector<SearchResult> out;
    if (fd == -1 || file_size == 0) return out;

    std::vector<char> buffer(file_size);
    if (!read_full_pread(fd, buffer.data(), file_size, 0)) {
        std::cerr << "Error reading file '" << filepath << "': " << std::strerror(errno) << std::endl;
        return out;
    }

    if (!config_.multifind) {
        const long pos = engine_.searchBuffer(buffer.data(), file_size);
        if (pos >= 0) {
            out.push_back(make_from_buffer(buffer.data(), file_size, pos, pos, getNeedle()));
        }
    } else {
        size_t next = 0;
        while (next < file_size) {
            const long pos = engine_.searchBuffer(buffer.data(), file_size, next);
            if (pos < 0) break;
            out.push_back(make_from_buffer(buffer.data(), file_size, pos, pos, getNeedle()));
            next = static_cast<size_t>(pos) + getNeedle().size();
        }
    }
    return out;
}

std::vector<SearchResult> FileSearcher::searchRange(int fd, size_t file_size, size_t start, size_t len, size_t overlap,
                                                    const std::string& filepath) const {
    std::vector<SearchResult> out;
    if (fd == -1 || len == 0 || start >= file_size) return out;

    long page_size = sysconf(_SC_PAGE_SIZE);
    if (page_size <= 0) page_size = 4096;

    const size_t aligned_offset = (start / static_cast<size_t>(page_size)) * static_cast<size_t>(page_size);
    const size_t map_offset_diff = start - aligned_offset;
    const size_t map_size = std::min(len + map_offset_diff + overlap, file_size - aligned_offset);

    if (map_size == 0) return out;

    char* mapped_addr = static_cast<char*>(mmap(NULL, map_size, PROT_READ, MAP_PRIVATE, fd, static_cast<off_t>(aligned_offset)));
    if (mapped_addr == MAP_FAILED) {
        std::cerr << "Error mmap file '" << filepath << "' at offset " << aligned_offset
                  << ": " << std::strerror(errno) << std::endl;
        return out;
    }

    char* search_start = mapped_addr + map_offset_diff;
    const size_t search_len = std::min(len + overlap, file_size - start);

    if (!config_.multifind) {
        const long pos = engine_.searchBuffer(search_start, search_len);
        if (pos >= 0) {
            if (static_cast<size_t>(pos) < len || (start + pos + getNeedle().size() == file_size)) {
                out.push_back(make_from_buffer(search_start, search_len, pos, start + pos, getNeedle()));
            }
        }
    } else {
        size_t local = 0;
        while (local < search_len) {
            const long rel = engine_.searchBuffer(search_start + local, search_len - local);
            if (rel < 0) break;
            const size_t match_in_chunk = local + static_cast<size_t>(rel);
            
            if (match_in_chunk < len || (start + match_in_chunk + getNeedle().size() == file_size)) {
                out.push_back(make_from_buffer(search_start, search_len, match_in_chunk, start + match_in_chunk, getNeedle()));
            } else {
                break;
            }
            local = match_in_chunk + getNeedle().size();
        }
    }

    munmap(mapped_addr, map_size);
    return out;
}
