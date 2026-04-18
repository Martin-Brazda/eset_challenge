#include "DirectoryWalker.h"
#include "ThreadPool.h"
#include <filesystem>
#include <iostream>
#include <memory>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

namespace fs = std::filesystem;

namespace {
struct FileDescriptor {
    int fd;
    explicit FileDescriptor(int f) : fd(f) {}
    ~FileDescriptor() { if (fd != -1) close(fd); }
};
}

DirectoryWalker::DirectoryWalker(const FileSearcher& fileSearcher, ThreadPool& pool) 
    : fileSearcher_(fileSearcher), pool_(pool) {}

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
        size_t size = fs::file_size(path, ec);
        if (!ec && size > 0) handle_file(path.string(), size);
    }
    
    if (fs::is_directory(path, ec)) {
        try {
            for (const auto& entry : fs::directory_iterator(path, fs::directory_options::skip_permission_denied)) {
                if (fs::is_symlink(entry.symlink_status())) continue;
                
                std::error_code size_ec;
                if (entry.is_regular_file(size_ec)) {
                    size_t size = entry.file_size(size_ec);
                    if (!size_ec && size > 0) handle_file(entry.path().string(), size);
                } else if (entry.is_directory(size_ec)) {
                    walk(entry.path().string());
                }
            }
        } catch (const std::exception&) {}
    }
    
    return false;
}

void DirectoryWalker::handle_file(const std::string& filepath, size_t file_size) const {
    const auto& config = fileSearcher_.getConfig();

    if (file_size > config.small_file_threshold) {
        // Large file: Now we open it because we need the FD for parallel chunking
        int fd = open(filepath.c_str(), O_RDONLY);
        if (fd == -1) return;

        auto fd_ptr = std::make_shared<FileDescriptor>(fd);
        const size_t chunk_size = config.chunking_threshold;
        const size_t overlap = fileSearcher_.getNeedle().size() > 0 ? fileSearcher_.getNeedle().size() - 1 : 0;

        for (size_t start = 0; start < file_size; start += chunk_size) {
            size_t len = std::min(chunk_size, file_size - start);
            size_t current_overlap = (start + len < file_size) ? overlap : 0;
            pool_.submit([this, fd_ptr, file_size, start, len, current_overlap, filepath]() {
                report_results(fileSearcher_.searchRange(fd_ptr->fd, file_size, start, len, current_overlap), filepath);
            });
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
            if (fd == -1) continue;
            report_results(fileSearcher_.searchSmallFile(fd, file.size, file.filepath), file.filepath);
            close(fd);
        }
    });
}