#include <DirectoryWalker.h>
#include <ThreadPool.h>
#include <filesystem>
#include <iostream>
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <cctype>
#include <cstdio>
#include <memory>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

namespace fs = std::filesystem;

namespace {
struct SharedFileDescriptor {
    int fd;
    std::atomic<size_t> pending_chunks;
    explicit SharedFileDescriptor(int file_fd, size_t chunks) : fd(file_fd), pending_chunks(chunks) {}
};
}  // namespace

DirectoryWalker::DirectoryWalker(const FileSearcher& fileSearcher, ThreadPool& pool)
    : fileSearcher_(fileSearcher), pool_(pool),
      max_open_large_fds_(std::max<size_t>(1, fileSearcher.getConfig().num_threads)) {}

void DirectoryWalker::acquire_large_fd_slot() const {
    std::unique_lock<std::mutex> lock(large_fd_mtx_);
    large_fd_cv_.wait(lock, [this]() { return open_large_fds_ < max_open_large_fds_; });
    ++open_large_fds_;
}

void DirectoryWalker::release_large_fd_slot() const {
    {
        std::lock_guard<std::mutex> lock(large_fd_mtx_);
        if (open_large_fds_ > 0) {
            --open_large_fds_;
        }
    }
    large_fd_cv_.notify_one();
}

void DirectoryWalker::escape_needle(std::string& context) const {
    std::string escaped;
    escaped.reserve(context.size());
    for (char c : context) {
        switch (c) {
            case '\n': escaped += "\\n"; break;
            case '\r': escaped += "\\r"; break;
            case '\t': escaped += "\\t"; break;
            case '\\': escaped += "\\\\"; break;
            default:
                if (!std::isprint(static_cast<unsigned char>(c))) {
                    char buf[5];
                    snprintf(buf, sizeof(buf), "\\x%02X", static_cast<unsigned char>(c));
                    escaped += buf;
                } else {
                    escaped += c;
                }
                break;
        }
    }
    context = std::move(escaped);
}

void DirectoryWalker::report_results(const std::vector<SearchResult>& results, const std::string& filepath) const {
    if (results.empty()) return;

    std::string filename = filepath.substr(filepath.find_last_of("/\\") + 1);
    
    std::lock_guard<std::mutex> lock(cout_mtx_);
    for (const auto& result : results) {
        if (result.offset < 0) continue;
        matches_++;
        std::string prefix = result.prefix;
        std::string suffix = result.suffix;
        escape_needle(prefix);
        escape_needle(suffix);

        std::cout << filename << "(" << result.offset << "): " << prefix << "..." << suffix << std::endl;
    }
}

bool DirectoryWalker::walk(const std::string& startPath) const {
#ifdef __linux__
    if (startPath.find("/proc") == 0 || startPath.find("/sys") == 0) return false;
#endif
    const fs::path path(startPath);
    std::error_code ec; 
    
    if (fs::is_regular_file(path, ec)) {
        if (ec) {
            std::cerr << "Error checking file '" << path.string() << "': " << ec.message() << std::endl;
            return false;
        }
        size_t size = fs::file_size(path, ec);
        if (ec) {
            std::cerr << "Error reading size for '" << path.string() << "': " << ec.message() << std::endl;
            return false;
        }
        if (size > 0) handle_file(path.string(), size);
    }
    
    if (fs::is_directory(path, ec)) {
        if (ec) {
            std::cerr << "Error checking directory '" << path.string() << "': " << ec.message() << std::endl;
            return false;
        }
        try {
            for (const auto& entry : fs::directory_iterator(path, fs::directory_options::skip_permission_denied)) {
                if (fs::is_symlink(entry.symlink_status())) continue;
                
                std::error_code size_ec;
                if (entry.is_regular_file(size_ec)) {
                    size_t size = entry.file_size(size_ec);
                    if (size_ec) {
                        std::cerr << "Error reading size for '" << entry.path().string() << "': "
                                  << size_ec.message() << std::endl;
                        continue;
                    }
                    if (size > 0) handle_file(entry.path().string(), size);
                } else if (entry.is_directory(size_ec)) {
                    walk(entry.path().string());
                } else if (size_ec) {
                    std::cerr << "Error inspecting '" << entry.path().string() << "': "
                              << size_ec.message() << std::endl;
                }
            }
        } catch (const std::exception& ex) {
            std::cerr << "Error traversing '" << path.string() << "': " << ex.what() << std::endl;
        }
    }
    
    return false;
}

void DirectoryWalker::handle_file(const std::string& filepath, size_t file_size) const {
    const auto& config = fileSearcher_.getConfig();

    if (file_size > config.small_file_threshold) {
        // Large file: share one FD across chunk workers while bounding open-large-file FDs globally.
        const size_t chunk_size = config.chunking_threshold;
        const size_t overlap = fileSearcher_.getNeedle().size() > 0 ? fileSearcher_.getNeedle().size() - 1 : 0;
        const size_t chunk_count = (file_size + chunk_size - 1) / chunk_size;
        if (chunk_count == 0) {
            return;
        }

        acquire_large_fd_slot();
        int fd = open(filepath.c_str(), O_RDONLY);
        if (fd == -1) {
            std::cerr << "Error opening file '" << filepath << "': " << std::strerror(errno) << std::endl;
            release_large_fd_slot();
            return;
        }

        auto shared_fd = std::make_shared<SharedFileDescriptor>(fd, chunk_count);
        try {
            for (size_t start = 0; start < file_size; start += chunk_size) {
                const size_t len = std::min(chunk_size, file_size - start);
                const size_t current_overlap = (start + len < file_size) ? overlap : 0;
                pool_.submit([this, shared_fd, file_size, start, len, current_overlap, filepath]() {
                    report_results(fileSearcher_.searchRange(shared_fd->fd, file_size, start, len, current_overlap, filepath), filepath);
                    if (shared_fd->pending_chunks.fetch_sub(1) == 1) {
                        close(shared_fd->fd);
                        release_large_fd_slot();
                    }
                });
            }
        } catch (...) {
            close(shared_fd->fd);
            release_large_fd_slot();
            throw;
        }
    } else {
        // Small file: Add to batch without opening
        bool should_flush = false;
        {
            std::lock_guard<std::mutex> lock(batch_mtx_);
            batch_buffer_.push_back({filepath, file_size});
            if (batch_buffer_.size() >= config.batch_size) {
                should_flush = true;
            }
        }
        if (should_flush) {
            flush_batch();
        }
    }
}

void DirectoryWalker::flush_batch() const {
    std::vector<FileInfo> batch;
    {
        std::lock_guard<std::mutex> lock(batch_mtx_);
        if (batch_buffer_.empty()) return;
        batch.swap(batch_buffer_);
    }

    pool_.submit([this, batch]() {
        for (const auto& file : batch) {
            int fd = open(file.filepath.c_str(), O_RDONLY);
            if (fd == -1) {
                std::cerr << "Error opening file '" << file.filepath << "': " << std::strerror(errno) << std::endl;
                continue;
            }
            report_results(fileSearcher_.searchSmallFile(fd, file.size, file.filepath), file.filepath);
            close(fd);
        }
    });
}