#pragma once

#include <filesystem>
#include <string>
#include <vector>

enum class FilesystemLocationResolution { RESOLVED, NOT_FOUND, AMBIGUOUS, INCOMPLETE };

struct FilesystemLocationCandidate {
    std::filesystem::path path;
    int score = 0;
};

struct FilesystemLocationResult {
    FilesystemLocationResolution status = FilesystemLocationResolution::NOT_FOUND;
    std::filesystem::path path;
    std::vector<FilesystemLocationCandidate> candidates;
    std::string message;
};

// Discovers existing directories from human-provided location clues. Discovery
// never authorizes mutation; callers must still use FilesystemPathPolicy.
class FilesystemLocationResolver {
public:
    FilesystemLocationResult resolve(const std::string& clues) const;
    FilesystemLocationResult resolve(const std::string& clues,
                                     const std::vector<std::filesystem::path>& roots) const;

private:
    std::vector<std::filesystem::path> discoveryRoots() const;
};
