#include "include/SearchEngine.h"
#include "include/FileSearcher.h"
#include "include/DirectoryWalker.h"
#include "include/ThreadPool.h"
#include "include/Config.h"
#include "include/Proggress.h"
#include <iostream>
#include <cstring>
#include <filesystem>

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <filepath> <string>\n";
        return 1;
    }

    if (std::filesystem::exists(argv[1]) == 0) {
        std::cerr << "Path does not exist\n";
        return 1;
    }

    if (strlen(argv[2]) > 128) {
        std::cerr << "String is too long (max 128 chars)\n";
        return 1;
    }

    std::string path_str = argv[1];
    std::string needle = argv[2];
    
    Config config;
    config.load_env(".env");

    if (config.num_threads == 0) {
        config.num_threads = 1;
    }

    ThreadPool pool(config.num_threads, config.queue_max_size);
    SearchEngine engine(needle);
    FileSearcher searcher(engine, config);
    DirectoryWalker walker(searcher, pool);

    /*
    CLI UI improvement. Decreases performance.

    ProgressAnimation progress;
    progress.start("Searching for '" + needle + "' in '" + path_str + "' ");
    */

    try {
        walker.walk(path_str);
        pool.stop();
        //progress.stop();

        if (walker.get_matches() > 0) {
            return 0;
        } else {
            std::cerr << "String not found\n";
            return 1;
        }
    } catch (const std::exception& e) {
        std::cerr << "Critical failure: " << e.what() << "\n";
        return 1;
    }
}
