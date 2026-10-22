#include "filesystem_location_resolver.hpp"

#include <chrono>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

namespace {
void check(const char* label, bool condition, int& failed) {
    std::cout << '[' << label << "] " << (condition ? "PASS" : "FAIL") << '\n';
    if (!condition) ++failed;
}
}

int main() {
    int failed = 0;
    const fs::path root = fs::temp_directory_path() /
        ("NovaLocationResolver_" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    const fs::path intended = root / "Projects" / "College" / "Sem 3";
    const fs::path other = root / "Archive" / "Sem 3";
    std::error_code error;
    fs::create_directories(intended, error);
    fs::create_directories(other, error);

    FilesystemLocationResolver resolver;
    const auto resolved = resolver.resolve("college projects semester 3 folder", {root});
    check("KEYWORDS_RESOLVE_CONTEXTUAL_FOLDER",
          resolved.status == FilesystemLocationResolution::RESOLVED &&
              fs::equivalent(resolved.path, intended, error), failed);

    const auto desktopHint = resolver.resolve("Desktop/projects/sem 3", {root});
    check("DESKTOP_ALIAS_IS_A_SEARCH_HINT_NOT_A_REQUIRED_PATH_SEGMENT",
          desktopHint.status == FilesystemLocationResolution::RESOLVED &&
              fs::equivalent(desktopHint.path, intended, error), failed);

    const auto ambiguous = resolver.resolve("sem 3", {root});
    check("AMBIGUOUS_KEYWORDS_ARE_NOT_GUESSED",
          ambiguous.status == FilesystemLocationResolution::AMBIGUOUS &&
              ambiguous.candidates.size() >= 2, failed);

    const auto missing = resolver.resolve("no such nova location", {root});
    check("MISSING_KEYWORDS_ARE_REPORTED", missing.status == FilesystemLocationResolution::NOT_FOUND, failed);

    fs::remove_all(root, error);
    return failed == 0 ? 0 : 1;
}
