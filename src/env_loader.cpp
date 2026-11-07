#include "env_loader.hpp"

#include <cstdlib>
#include <fstream>
#include <stdexcept>
#include <string>
#include <system_error>

namespace {
std::string trim(const std::string& value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

bool validName(const std::string& name) {
    if (name.empty() || !(name[0] == '_' || (name[0] >= 'A' && name[0] <= 'Z') ||
                          (name[0] >= 'a' && name[0] <= 'z'))) return false;
    for (const char c : name) {
        if (!(c == '_' || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
              (c >= '0' && c <= '9'))) return false;
    }
    return true;
}

std::filesystem::path findEnvironmentFile(const std::filesystem::path& start, bool isExecutablePath) {
    std::error_code error;
    auto directory = std::filesystem::absolute(start, error);
    if (error) throw std::runtime_error("Could not resolve the Nova executable directory.");
    if (isExecutablePath) directory = directory.parent_path();

    while (!directory.empty()) {
        const auto candidate = directory / ".env";
        if (std::filesystem::exists(candidate, error)) {
            if (error) throw std::runtime_error("Could not inspect the Nova environment file.");
            return candidate;
        }
        if (error) throw std::runtime_error("Could not inspect the Nova environment file.");
        const auto parent = directory.parent_path();
        if (parent == directory) break;
        directory = parent;
    }
    return {};
}

void setValue(const std::string& name, const std::string& value) {
#ifdef _WIN32
    if (_putenv_s(name.c_str(), value.c_str()) != 0)
        throw std::runtime_error("Could not set environment variable " + name + ".");
#else
    if (setenv(name.c_str(), value.c_str(), 1) != 0)
        throw std::runtime_error("Could not set environment variable " + name + ".");
#endif
}

void readEnvironmentFile(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("Could not open Nova environment file: " + path.string());

    std::string line;
    size_t lineNumber = 0;
    while (std::getline(input, line)) {
        ++lineNumber;
        if (lineNumber == 1 && line.size() >= 3 &&
            static_cast<unsigned char>(line[0]) == 0xEF &&
            static_cast<unsigned char>(line[1]) == 0xBB &&
            static_cast<unsigned char>(line[2]) == 0xBF) line.erase(0, 3);

        const auto entry = trim(line);
        if (entry.empty() || entry[0] == '#') continue;
        const auto separator = entry.find('=');
        if (separator == std::string::npos)
            throw std::runtime_error("Invalid entry in Nova environment file at line " + std::to_string(lineNumber) + ".");

        const auto name = trim(entry.substr(0, separator));
        if (!validName(name))
            throw std::runtime_error("Invalid variable name in Nova environment file at line " + std::to_string(lineNumber) + ".");

        auto value = trim(entry.substr(separator + 1));
        if (value.size() >= 2 &&
            ((value.front() == '"' && value.back() == '"') ||
             (value.front() == '\'' && value.back() == '\''))) {
            value = value.substr(1, value.size() - 2);
        }
        setValue(name, value);
    }
    if (input.bad()) throw std::runtime_error("Could not read Nova environment file: " + path.string());
}
}

void loadEnvironmentFile(const std::filesystem::path& executablePath) {
    const auto currentDirectoryFile = findEnvironmentFile(std::filesystem::current_path(), false);
    if (!currentDirectoryFile.empty()) {
        readEnvironmentFile(currentDirectoryFile);
        return;
    }
    const auto executableDirectoryFile = findEnvironmentFile(executablePath, true);
    if (!executableDirectoryFile.empty()) readEnvironmentFile(executableDirectoryFile);
}
