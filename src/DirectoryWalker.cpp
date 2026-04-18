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
        int fd = open(startPath.c_str(), O_RDONLY);
        if (fd != -1) {
            struct stat st;
            if (fstat(fd, &st) == 0 && st.st_size > 0) {
                const size_t file_size = static_cast<size_t>(st.st_size);
                auto fd_ptr = std::make_shared<FileDescriptor>(fd);
                const auto& config = fileSearcher_.getConfig();
                const size_t overlap = fileSearcher_.getNeedle().size() > 0 ? fileSearcher_.getNeedle().size() - 1 : 0;

                if (file_size <= config.small_file_threshold) {
                    pool_.submit([this, fd_ptr, file_size, startPath]() {
                        report_results(fileSearcher_.searchSmallFile(fd_ptr->fd, file_size, startPath), startPath);
                    });
                } else {
                    const size_t chunk_size = config.chunking_threshold;
                    for (size_t start = 0; start < file_size; start += chunk_size) {
                        size_t len = std::min(chunk_size, file_size - start);
                        size_t current_overlap = (start + len < file_size) ? overlap : 0;
                        pool_.submit([this, fd_ptr, file_size, start, len, current_overlap, startPath]() {
                            report_results(fileSearcher_.searchRange(fd_ptr->fd, file_size, start, len, current_overlap), startPath);
                        });
                    }
                }
            } else {
                close(fd);
            }
        }
    }
    
    if (fs::is_directory(path, ec)) {
        try {
            for (const auto& entry : fs::directory_iterator(path, fs::directory_options::skip_permission_denied)) {
                if (fs::is_symlink(entry.symlink_status())) continue;
                walk(entry.path().string());
            }
        } catch (const std::exception&) {}
    }
    
    return false;
}
