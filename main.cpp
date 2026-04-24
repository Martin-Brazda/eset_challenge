#include <SearchEngine.h>
#include <FileSearcher.h>
#include <DirectoryWalker.h>
#include <ThreadPool.h>
#include <Config.h>
#include <Progress.h>
#include <iostream>
#include <cstring>
#include <filesystem>
#include <CLIParser.h>

int main(int argc, char* argv[]) {
    Config config;
    CLIParser parser;
    ParseResult result = parser.parse(argc, argv);
    if (result == ParseResult::Help) {
        return 0;
    }
    if (result == ParseResult::Error) {
        return 1;
    }

    if (!std::filesystem::exists(parser.filepath)) {
        std::cerr << "Path does not exist\n";
        return 1;
    }

    std::string path_str = parser.filepath;
    std::string needle = parser.needle;
    
    config.load_env(".env");

    if (config.num_threads <= 0) {
        std::cerr << "Number of threads must be a positive integer\n";
        return 1;
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
        walker.flush_batch();
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
