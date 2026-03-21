#include "DirectoryWalker.h"
#include "ThreadPool.h"
#include <filesystem>
#include <iostream>
#include <future>

namespace fs = std::filesystem;

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

bool DirectoryWalker::walk(const std::string& startPath) const {
#ifdef __linux__
    if (startPath.find("/proc") == 0 || startPath.find("/sys") == 0) return false;
#endif
    const fs::path path(startPath);
    std::error_code ec; 
    
    if (fs::is_regular_file(path, ec)) {
        try {
            pool_.submit([this, startPath]() {
                try {
                    SearchResult result = fileSearcher_.search(startPath);
                    if (result.offset != -1) {
                        matches_++;
                        std::string prefix = result.prefix;
                        std::string suffix = result.suffix;
                        escape_needle(prefix);
                        escape_needle(suffix);

                        std::string file = startPath.substr(startPath.find_last_of("/\\") + 1);
                        
                        std::lock_guard<std::mutex> lock(cout_mtx_);
                        std::cout << file << "(" << result.offset << "): " << prefix << "..." << suffix << std::endl;
                    }
                } catch (...) {}
            });
        } catch (const std::exception& e) {
            std::cerr << "Error submitting task: " << e.what() << std::endl;
        }
    }
    
    if (ec) {
        std::lock_guard<std::mutex> lock(cout_mtx_);
        std::cerr << "Error with path " << startPath << ": " << ec.message() << std::endl;
    }

    if (fs::is_directory(path, ec)) {
        try {
            for (const auto& entry : fs::directory_iterator(path, fs::directory_options::skip_permission_denied)) {
                if (fs::is_symlink(entry.symlink_status())) continue;
                walk(entry.path().string());
            }
        } catch (const std::exception&) {
            // ignore unreadable directories natively
        }
    }
    
    return false;
}
