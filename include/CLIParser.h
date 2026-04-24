#pragma once
#include <string>
#include <Config.h>

enum class ParseResult {
    Ok,
    Help,
    Error,
};

/**
 * @brief Command-line argument parser for the application.
 */
class CLIParser {
public:
    std::string filepath;
    std::string needle;

    /**
     * @brief Parses command-line arguments and loads the configuration.
     * @param argc Argument count.
     * @param argv Argument vector.
     * @return ParseResult::Ok if parsing succeeded and execution should continue.
     */
    ParseResult parse(int argc, char* argv[]);
    
    /**
     * @brief Prints the usage information.
     */
    void print_usage(const char* program_name) const;
};