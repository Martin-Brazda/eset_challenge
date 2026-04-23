#include <CLIParser.h>
#include <iostream>
#include <cstring>
#include <cstdlib>

void CLIParser::print_usage(const char* program_name) const {
    std::cout << "Usage: " << program_name << " [options] <filepath> <string>\n\n";
    std::cout << "Arguments:\n";
    std::cout << "  <filepath>           The directory or file path to search in\n";
    std::cout << "  <string>             The string to search for (max 128 characters)\n\n";
    std::cout << "Options:\n";
    std::cout << "  -h, --help           Show this help message and exit\n";
    std::cout << "  -t, --threads <N>    Override the number of worker threads\n";
    std::cout << "  -e, --env <path>     Specify a custom path to the .env file\n";
}

bool CLIParser::parse(int argc, char* argv[]) {
    int positional_count = 0;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "-h") == 0 || std::strcmp(argv[i], "--help") == 0) {
            help_requested = true;
            print_usage(argv[0]);
            return false; // Stop execution, help handled
        } else if (std::strcmp(argv[i], "-t") == 0 || std::strcmp(argv[i], "--threads") == 0) {
            if (i + 1 < argc) {
                threads = std::atoi(argv[++i]);
                if (*threads <= 0) {
                    std::cerr << "Error: Number of threads must be a positive integer.\n";
                    return false;
                }
            } else {
                std::cerr << "Error: --threads requires an argument.\n";
                return false;
            }
        } else if (std::strcmp(argv[i], "-e") == 0 || std::strcmp(argv[i], "--env") == 0) {
            if (i + 1 < argc) {
                env_path = argv[++i];
            } else {
                std::cerr << "Error: --env requires an argument.\n";
                return false;
            }
        } else if (argv[i][0] == '-') {
            std::cerr << "Error: Unknown option " << argv[i] << "\n";
            std::cerr << "Try '" << argv[0] << " --help' for more information.\n";
            return false;
        } else {
            if (positional_count == 0) {
                filepath = argv[i];
            } else if (positional_count == 1) {
                needle = argv[i];
            } else {
                std::cerr << "Error: Too many positional arguments.\n";
                std::cerr << "Try '" << argv[0] << " --help' for more information.\n";
                return false;
            }
            positional_count++;
        }
    }

    if (positional_count < 2) {
        std::cerr << "Error: Missing required positional arguments.\n";
        std::cerr << "Try '" << argv[0] << " --help' for more information.\n";
        return false;
    }

    if (needle.length() > 128) {
        std::cerr << "Error: String is too long (max 128 chars)\n";
        return false;
    }
    if (needle.empty()) {
        std::cerr << "Error: String is empty\n";
        return false;
    }

    return true;
}
