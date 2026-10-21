#pragma once

#include <filesystem>
#include <string>
#include <vector>

// Resolution and authorization deliberately live together, but remain two
// separate decisions: a well-formed path is not automatically safe to mutate.
enum class FilesystemAccess {
    DISCOVER,
    CREATE,
    WRITE,
    MOVE,
    COPY,
    RENAME,
    DELETE_RESOURCE
};

enum class FilesystemPathError {
    NONE,
    MALFORMED_PATH,
    UNKNOWN_LOCATION,
    NOT_FOUND,
    ACCESS_DENIED,
    PROTECTED_LOCATION,
    IO_ERROR
};

struct FilesystemPathResult {
    bool ok = false;
    std::filesystem::path path;
    FilesystemPathError error = FilesystemPathError::NONE;
    std::string message;
};

class FilesystemPathPolicy {
public:
    // Resolves known-folder aliases (desktop, documents, downloads), absolute
    // paths, and a relative path only when an explicit resolved parent is given.
    FilesystemPathResult resolve(const std::string& value,
                                 const std::filesystem::path& parent = {}) const;

    // Applies operation-specific policy to the physical (reparse-point-aware)
    // path. A non-existent write target is evaluated through its real parent.
    FilesystemPathResult authorize(const std::filesystem::path& path,
                                   FilesystemAccess access) const;

    static std::string errorName(FilesystemPathError error);

private:
    std::filesystem::path knownFolder(const std::string& alias) const;
    std::filesystem::path canonicalForPolicy(const std::filesystem::path& path,
                                             std::error_code& error) const;
    bool isProtected(const std::filesystem::path& path) const;
    std::vector<std::filesystem::path> protectedRoots() const;
};
