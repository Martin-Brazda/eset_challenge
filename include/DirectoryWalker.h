#pragma once
#include <FileSearcher.h>
#include <ThreadPool.h>
#include <string>
#include <mutex>
#include <atomic>
#include <condition_variable>

struct FileInfo {
    std::string filepath;
    size_t size;
};

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
    
    /** @brief Flushes the batch buffer to the ThreadPool. */
    void flush_batch() const;

    /** @brief Returns total number of matches found across all files. */
    int get_matches() const { return matches_; }
    
private:
    void escape_needle(std::string& str) const;
    void report_results(const std::vector<SearchResult>& results, const std::string& filepath) const;
    void handle_file(const std::string& filepath, size_t file_size) const;
    void acquire_large_fd_slot() const;
    void release_large_fd_slot() const;

    const FileSearcher& fileSearcher_;
    ThreadPool& pool_;
    mutable std::mutex cout_mtx_;
    mutable std::atomic<int> matches_{0};
    mutable std::mutex batch_mtx_;
    mutable std::vector<FileInfo> batch_buffer_;
    mutable std::mutex large_fd_mtx_;
    mutable std::condition_variable large_fd_cv_;
    mutable size_t open_large_fds_{0};
    size_t max_open_large_fds_{1};
};
