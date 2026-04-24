#include <CLIParser.h>
#include <iostream>
#include <cstring>

void CLIParser::print_usage(const char* program_name) const {
    std::cout << "Usage: " << program_name << " <filepath> <string>\n\n";
    std::cout << "Arguments:\n";
    std::cout << "  <filepath>           The directory or file path to search in\n";
    std::cout << "  <string>             The string to search for (max 128 characters)\n";
    std::cout << "Options:\n";
    std::cout << "  -h, --help           Show this help message and exit\n";
}

ParseResult CLIParser::parse(int argc, char* argv[]) {
    if (argc == 2 && (std::strcmp(argv[1], "-h") == 0 || std::strcmp(argv[1], "--help") == 0)) {
        print_usage(argv[0]);
        return ParseResult::Help;
    }

    if (argc != 3) {
        std::cerr << "Error: Expected exactly two arguments.\n";
        std::cerr << "Try '" << argv[0] << " --help' for more information.\n";
        return ParseResult::Error;
    }

    filepath = argv[1];
    needle = argv[2];

    if (needle.length() > 128) {
        std::cerr << "Error: String is too long (max 128 chars)\n";
        return ParseResult::Error;
    }
    if (needle.empty()) {
        std::cerr << "Error: String is empty\n";
        return ParseResult::Error;
    }

    return ParseResult::Ok;
}
