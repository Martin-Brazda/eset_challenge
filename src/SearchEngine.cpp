#include "SearchEngine.h"
#include <algorithm>
#include <functional>

SearchEngine::SearchEngine(const std::string& needle) : needle_(needle), searcher_(needle_.begin(), needle_.end()) {}

long SearchEngine::searchBuffer(const char* buffer, size_t size, size_t start) const {
    if (start >= size) {
        return -1;
    }
    auto it = std::search(buffer + start, buffer + size, searcher_);

    if (it != buffer + size) {
        return static_cast<long>(std::distance(buffer, it));
    }
    return -1;
}