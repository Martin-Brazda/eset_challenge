#pragma once
#include "SearchEngine.h"
#include "Config.h"
#include <string>
#include <vector>

/**
 * @brief Represents a successful search result.
 */
struct SearchResult {
    long offset;         ///< Zero-based byte offset in the file.
    std::string prefix;  ///< Up to 3 characters before the match.
    std::string suffix;  ///< Up to 3 characters after the match.
};

/**
 * @brief Handles file-level streaming and buffered searching.
 *
 * Reads files in chunks to minimize memory usage while ensuring
 * that needles spanning across buffer boundaries are correctly identified.
 *
 * If built with -DMULTIFIND, every non-overlapping occurrence is reported.
 * (advance by needle size after each hit). Otherwise only the first hit per file.
 */
class FileSearcher {
public:
    explicit FileSearcher(const SearchEngine& engine, const Config& config);

    /**
     * @return All matches in the file (empty if none). Without MULTIFIND, at most one element.
     */
    /**
     * @brief Searches a specific range of a file.
     * @param fd Open file descriptor.
     * @param file_size Total size of the file.
     * @param start Byte offset to start searching from.
     * @param len Number of bytes to search in this task.
     * @param overlap Number of bytes to overlap with the next chunk.
     */
    std::vector<SearchResult> searchRange(int fd, size_t file_size, size_t start, size_t len, size_t overlap) const;

    const std::string& getNeedle() const { return engine_.getNeedle(); }
    bool logging_enabled() const { return config_.logging_enabled; }
    const Config& getConfig() const { return config_; }

    /**
     * @brief Searches a small file by reading it entirely into memory.
     */
    std::vector<SearchResult> searchSmallFile(int fd, size_t file_size, const std::string& filepath) const;

private:

    const SearchEngine& engine_;
    const Config& config_;
};
