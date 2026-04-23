#pragma once
#include <string>
#include <optional>
#include <Config.h>

/**
 * @brief Command-line argument parser for the application.
 */
class CLIParser {
public:
    std::string filepath;
    std::string needle;
    
    // Optional flags
    bool help_requested = false;
    std::optional<int> threads;
    std::optional<std::string> env_path;

    /**
     * @brief Parses command-line arguments and loads the configuration.
     * @param argc Argument count.
     * @param argv Argument vector.
     * @return True if parsing succeeded and execution should continue, false otherwise.
     */
    bool parse(int argc, char* argv[]);
    
    /**
     * @brief Prints the usage information.
     */
    void print_usage(const char* program_name) const;
};