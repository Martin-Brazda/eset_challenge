#pragma once
#include "SearchEngine.h"
#include "Config.h"
#include <string>

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
 */
class FileSearcher {
public:
    /**
     * @brief Constructs a FileSearcher with a search engine and configuration.
     */
    explicit FileSearcher(const SearchEngine& engine, const Config& config);
    
    /**
     * @brief Performs a buffered search on a specific file.
     * @param filepath Absolute or relative path to the file.
     * @return A SearchResult object. offset is -1 if not found.
     */
    SearchResult search(const std::string& filepath) const;
    
    const std::string& getNeedle() const { return engine_.getNeedle(); }
    
private:
    const SearchEngine& engine_;
    const Config& config_;
};
