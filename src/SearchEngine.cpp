#include <SearchEngine.h>
#include <algorithm>
#include <cstring>
#include <functional>

SearchEngine::SearchEngine(const std::string& needle) : needle_(needle), searcher_(needle_.begin(), needle_.end()) {}

std::optional<size_t> SearchEngine::searchBuffer(const char* buffer, size_t size, size_t start) const {
    if (start >= size) return std::nullopt;

    const size_t needle_len = needle_.size();
    const char* data = buffer + start;
    const char* end = buffer + size;

    if (needle_len == 1) {
        const void* p = memchr(data, needle_[0], end - data);
        if (!p) {
            return std::nullopt;
        }
        return static_cast<size_t>(static_cast<const char*>(p) - buffer);
    }

    if (needle_len <= 16) {
        const char first = needle_[0];

        while (data < end) {
            data = static_cast<const char*>(memchr(data, first, end - data));
            if (!data) return std::nullopt;

            if (data + needle_len <= end &&
                memcmp(data, needle_.data(), needle_len) == 0) {
                return static_cast<size_t>(data - buffer);
            }
            data++;
        }
        return std::nullopt;
    }

    auto it = std::search(buffer + start, buffer + size, searcher_);
    return (it != buffer + size) ? std::optional<size_t>(static_cast<size_t>(it - buffer)) : std::nullopt;
}