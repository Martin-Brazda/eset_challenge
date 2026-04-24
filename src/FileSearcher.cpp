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

bool read_full_pread(int fd, char* buf, size_t count, off_t offset) {
    size_t total = 0;
    while (total < count) {
        ssize_t n = ::pread(fd, buf + total, count - total, offset + static_cast<off_t>(total));
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
} // namespace

SearchResult make_from_buffer(const char* buf, size_t buf_len, size_t local_pos, size_t global_pos, size_t needle_len) {
    const size_t start_idx = local_pos < 3 ? 0 : local_pos - 3;
    const size_t end_idx = std::min(buf_len, local_pos + needle_len + 3);
    return {static_cast<long>(global_pos),
            std::string(buf + start_idx, buf + local_pos),
            std::string(buf + local_pos + needle_len, buf + end_idx)};
}

}  // namespace

FileSearcher::FileSearcher(const SearchEngine& engine, const Config& config)
    : engine_(engine), config_(config) {}

std::vector<SearchResult> FileSearcher::searchSmallFile(int fd, size_t file_size,
                                                        const std::string& filepath) const {
    std::vector<SearchResult> out;
    if (fd == -1 || file_size == 0) return out;
    const size_t needle_len = getNeedle().size();

    std::vector<char> buffer(file_size);
    if (!read_full_pread(fd, buffer.data(), file_size, 0)) {
        std::cerr << "Error reading file '" << filepath << "': " << std::strerror(errno) << std::endl;
        return out;
    }

    if (!config_.multifind) {
        const auto pos = engine_.searchBuffer(buffer.data(), file_size);
        if (pos.has_value()) {
            out.push_back(make_from_buffer(buffer.data(), file_size, *pos, *pos, needle_len));
        }
    } else {
        size_t next = 0;
        while (next < file_size) {
            const auto pos = engine_.searchBuffer(buffer.data(), file_size, next);
            if (!pos.has_value()) break;
            out.push_back(make_from_buffer(buffer.data(), file_size, *pos, *pos, needle_len));
            next = *pos + needle_len;
        }
    }
    return out;
}

std::vector<SearchResult> FileSearcher::searchRange(int fd, size_t file_size, size_t start, size_t len, size_t overlap,
                                                    const std::string& filepath) const {
    std::vector<SearchResult> out;
    if (fd == -1 || len == 0 || start >= file_size) return out;
    const size_t needle_len = getNeedle().size();

    constexpr size_t kContextSize = 3;
    static const size_t page_size = [] {
        long ps = sysconf(_SC_PAGE_SIZE);
        return ps > 0 ? static_cast<size_t>(ps) : static_cast<size_t>(4096);
    }();

    const size_t context_before = std::min(kContextSize, start);
    const size_t window_start = start - context_before;
    const size_t need_after = overlap + kContextSize;
    const size_t window_len = std::min(context_before + len + need_after, file_size - window_start);

    const size_t aligned_offset = (window_start / page_size) * page_size;
    const size_t map_offset_diff = window_start - aligned_offset;
    const size_t map_size = std::min(window_len + map_offset_diff, file_size - aligned_offset);

    if (map_size == 0) return out;

    char* mapped_addr = static_cast<char*>(mmap(NULL, map_size, PROT_READ, MAP_PRIVATE, fd, static_cast<off_t>(aligned_offset)));
    if (mapped_addr == MAP_FAILED) {
        std::cerr << "Error mmap file '" << filepath << "' at offset " << aligned_offset
                  << ": " << std::strerror(errno) << std::endl;
        return out;
    }

    char* window_ptr = mapped_addr + map_offset_diff;
    char* search_start = window_ptr + context_before;
    const size_t search_len = std::min(len + overlap, file_size - start);

    if (!config_.multifind) {
        const auto pos = engine_.searchBuffer(search_start, search_len);
        if (pos.has_value()) {
            if (*pos < len || (start + *pos + needle_len == file_size)) {
                out.push_back(make_from_buffer(window_ptr, window_len,
                                               context_before + *pos,
                                               start + *pos, needle_len));
            }
        }
    } else {
        size_t local = 0;
        while (local < search_len) {
            const auto rel = engine_.searchBuffer(search_start + local, search_len - local);
            if (!rel.has_value()) break;
            const size_t match_in_chunk = local + *rel;
            
            if (match_in_chunk < len || (start + match_in_chunk + needle_len == file_size)) {
                out.push_back(make_from_buffer(window_ptr, window_len,
                                               context_before + match_in_chunk,
                                               start + match_in_chunk, needle_len));
            } else {
                break;
            }
            local = match_in_chunk + needle_len;
        }
    }

    munmap(mapped_addr, map_size);
    return out;
}
