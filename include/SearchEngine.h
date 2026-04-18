#pragma once
#include <string>
#include <vector>
#include <functional>

/**
 * @brief Core searching logic using Boyer-Moore algorithm.
 * 
 * Reuses a single std::boyer_moore_searcher instance to maximize performance
 * by avoiding skip-table recalculation in inner loops.
 */
class SearchEngine {
public:
    /**
     * @brief Constructs a SearchEngine for a specific needle string.
     * @param needle The string to search for (max 128 chars).
     */
    explicit SearchEngine(const std::string& needle);
    
    /**
     * @brief Searches a memory buffer for the needle.
     * @param buffer Pointer to the contiguous memory to search.
     * @param size Size of the buffer in bytes.
     * @param start First index to consider (for repeated scans).
     * @return Offset of the match relative to @p buffer, or -1 if not found.
     */
    long searchBuffer(const char* buffer, size_t size, size_t start = 0) const;
    
    const std::string& getNeedle() const { return needle_; }

private:
    std::string needle_;
    std::boyer_moore_searcher<std::string::const_iterator> searcher_;
};