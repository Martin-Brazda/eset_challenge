#include "SearchEngine.h"
#include <algorithm>
#include <functional>

SearchEngine::SearchEngine(const std::string& needle) : needle_(needle), searcher_(needle_.begin(), needle_.end()) {}

long SearchEngine::searchBuffer(const char* buffer, size_t size) const {
    auto it = std::search(buffer, buffer + size, searcher_);
                          
    if (it != buffer + size) {
        return std::distance(buffer, it);
    }
    return -1;
}
