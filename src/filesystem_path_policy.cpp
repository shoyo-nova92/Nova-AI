#include "filesystem_path_policy.hpp"

#include <windows.h>
#include <shlobj.h>

#include <algorithm>
#include <cctype>
#include <system_error>

namespace fs = std::filesystem;

namespace {
std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

bool containsTraversal(const fs::path& value) {
    for (const auto& component : value) {
        if (component == "..") return true;
    }
    return false;
}

bool sameOrDescendant(const fs::path& path, const fs::path& root) {
    auto pathIt = path.begin();
    auto rootIt = root.begin();
    for (; rootIt != root.end(); ++rootIt, ++pathIt) {
        if (pathIt == path.end() || lower(pathIt->string()) != lower(rootIt->string())) return false;
    }
    return true;
}

fs::path knownFolder(REFKNOWNFOLDERID id) {
    PWSTR raw = nullptr;
    const HRESULT result = SHGetKnownFolderPath(id, 0, nullptr, &raw);
    if (FAILED(result) || raw == nullptr) return {};
    fs::path path(raw);
    CoTaskMemFree(raw);
    return path;
}
} // namespace

FilesystemPathResult FilesystemPathPolicy::resolve(const std::string& value,
                                                    const fs::path& parent) const {
    if (value.empty()) {
        return {false, {}, FilesystemPathError::MALFORMED_PATH, "Path is empty."};
    }

    std::string normalized = value;
    std::replace(normalized.begin(), normalized.end(), '/', '\\');
    const auto slash = normalized.find('\\');
    const std::string first = lower(normalized.substr(0, slash));
    fs::path base = knownFolder(first);
    fs::path candidate;

    if (!base.empty()) {
        const std::string suffix = slash == std::string::npos ? "" : normalized.substr(slash + 1);
        const fs::path relative(suffix);
        if (containsTraversal(relative)) {
            return {false, {}, FilesystemPathError::MALFORMED_PATH,
                "Known-folder paths must not contain '..'."};
        }
        candidate = base / relative;
    } else {
        const fs::path supplied(normalized);
        if (supplied.is_absolute()) {
            candidate = supplied;
        } else if (!parent.empty()) {
            if (containsTraversal(supplied)) {
                return {false, {}, FilesystemPathError::MALFORMED_PATH,
                    "Relative paths must not contain '..'."};
            }
            candidate = parent / supplied;
        } else {
            return {false, {}, FilesystemPathError::UNKNOWN_LOCATION,
                "Path must be absolute, a supported known-folder path, or relative to an explicit parent."};
        }
    }

    candidate = candidate.lexically_normal();
    if (!candidate.is_absolute()) {
        return {false, {}, FilesystemPathError::MALFORMED_PATH,
            "Resolved path is not absolute."};
    }
    return {true, candidate, FilesystemPathError::NONE, {}};
}

FilesystemPathResult FilesystemPathPolicy::authorize(const fs::path& path,
                                                      FilesystemAccess access) const {
    if (path.empty() || !path.is_absolute()) {
        return {false, {}, FilesystemPathError::MALFORMED_PATH,
            "Authorization requires an absolute path."};
    }

    std::error_code error;
    const fs::path physical = canonicalForPolicy(path, error);
    if (error) {
        return {false, {}, FilesystemPathError::IO_ERROR,
            "Could not canonicalize path for policy evaluation: " + error.message()};
    }
    if (access != FilesystemAccess::DISCOVER &&
        physical == physical.root_path()) {
        return {false, physical, FilesystemPathError::PROTECTED_LOCATION,
            "Mutating a filesystem volume root is not permitted: " + physical.string()};
    }
    if (isProtected(physical) && access != FilesystemAccess::DISCOVER) {
        return {false, physical, FilesystemPathError::PROTECTED_LOCATION,
            "This operation is blocked for a protected Windows or application location: " + physical.string()};
    }
    return {true, physical, FilesystemPathError::NONE, {}};
}

std::string FilesystemPathPolicy::errorName(FilesystemPathError error) {
    switch (error) {
        case FilesystemPathError::NONE: return "none";
        case FilesystemPathError::MALFORMED_PATH: return "malformed_path";
        case FilesystemPathError::UNKNOWN_LOCATION: return "unknown_location";
        case FilesystemPathError::NOT_FOUND: return "not_found";
        case FilesystemPathError::ACCESS_DENIED: return "access_denied";
        case FilesystemPathError::PROTECTED_LOCATION: return "protected_location";
        case FilesystemPathError::IO_ERROR: return "io_error";
    }
    return "unknown";
}

fs::path FilesystemPathPolicy::knownFolder(const std::string& alias) const {
    if (alias == "desktop") return ::knownFolder(FOLDERID_Desktop);
    if (alias == "documents") return ::knownFolder(FOLDERID_Documents);
    if (alias == "downloads") return ::knownFolder(FOLDERID_Downloads);
    return {};
}

fs::path FilesystemPathPolicy::canonicalForPolicy(const fs::path& path,
                                                   std::error_code& error) const {
    error.clear();
    fs::path existing = path;
    fs::path suffix;
    while (!existing.empty() && !fs::exists(existing, error)) {
        if (error) return {};
        suffix = existing.filename() / suffix;
        existing = existing.parent_path();
    }
    if (existing.empty()) {
        error = std::make_error_code(std::errc::no_such_file_or_directory);
        return {};
    }
    const fs::path physicalBase = fs::weakly_canonical(existing, error);
    if (error) return {};
    return (physicalBase / suffix).lexically_normal();
}

bool FilesystemPathPolicy::isProtected(const fs::path& path) const {
    for (const auto& root : protectedRoots()) {
        std::error_code error;
        const fs::path physicalRoot = fs::weakly_canonical(root, error);
        if (!error && sameOrDescendant(path, physicalRoot)) return true;
    }
    return false;
}

std::vector<fs::path> FilesystemPathPolicy::protectedRoots() const {
    std::vector<fs::path> roots;
    wchar_t windows[MAX_PATH]{};
    wchar_t system[MAX_PATH]{};
    wchar_t programFiles[MAX_PATH]{};
    if (GetWindowsDirectoryW(windows, MAX_PATH)) roots.emplace_back(windows);
    if (GetSystemDirectoryW(system, MAX_PATH)) roots.emplace_back(system);
    if (GetEnvironmentVariableW(L"ProgramFiles", programFiles, MAX_PATH)) roots.emplace_back(programFiles);
    if (GetEnvironmentVariableW(L"ProgramFiles(x86)", programFiles, MAX_PATH)) roots.emplace_back(programFiles);
    return roots;
}
