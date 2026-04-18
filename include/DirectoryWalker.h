#pragma once
#include "FileSearcher.h"
#include "ThreadPool.h"
#include <string>
#include <mutex>
#include <atomic>

/**
 * @brief Traverses directory trees and submits files to the ThreadPool.
 */
class DirectoryWalker {
public:
    /**
     * @brief Constructs a DirectoryWalker.
     * @param fileSearcher Instance of FileSearcher to use for scanning.
     * @param pool ThreadPool to use for parallel processing.
     */
    explicit DirectoryWalker(const FileSearcher& fileSearcher, ThreadPool& pool);
    
    /**
     * @brief Recursively walks a path and queues files for searching.
     * @param startPath Absolute or relative path to a file or directory.
     * @return False (the match count is tracked via an atomic internally).
     */
    bool walk(const std::string& startPath) const;
    
    /** @brief Returns total number of matches found across all files. */
    int get_matches() const { return matches_; }
    
private:
    void escape_needle(std::string& str) const;
    void report_results(const std::vector<SearchResult>& results, const std::string& filepath) const;
    
    const FileSearcher& fileSearcher_;
    ThreadPool& pool_;
    mutable std::mutex cout_mtx_;
    mutable std::atomic<int> matches_{0};
};
