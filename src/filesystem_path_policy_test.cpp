#include "filesystem_path_policy.hpp"

#include <windows.h>
#include <iostream>

namespace fs = std::filesystem;

namespace {
int passed = 0;
int failed = 0;
void check(const char* label, bool condition) {
    std::cout << '[' << label << "] " << (condition ? "PASS" : "FAIL") << '\n';
    condition ? ++passed : ++failed;
}
}

int main() {
    FilesystemPathPolicy policy;

    const auto desktop = policy.resolve("desktop");
    check("KNOWN_DESKTOP_RESOLVES", desktop.ok && desktop.path.is_absolute());

    const auto documents = policy.resolve("documents");
    check("KNOWN_DOCUMENTS_RESOLVES", documents.ok && documents.path.is_absolute());

    const auto absolute = policy.resolve(documents.path.string());
    check("ABSOLUTE_PATH_RESOLVES", absolute.ok && absolute.path.is_absolute());

    const auto traversal = policy.resolve("desktop/../outside");
    check("KNOWN_FOLDER_TRAVERSAL_REJECTED",
          !traversal.ok && traversal.error == FilesystemPathError::MALFORMED_PATH);

    wchar_t windowsDirectory[MAX_PATH]{};
    GetWindowsDirectoryW(windowsDirectory, MAX_PATH);
    const auto protectedWrite = policy.resolve(fs::path(windowsDirectory).string());
    const auto authorization = protectedWrite.ok
        ? policy.authorize(protectedWrite.path, FilesystemAccess::WRITE)
        : FilesystemPathResult{};
    check("WINDOWS_WRITE_REJECTED",
          !authorization.ok && authorization.error == FilesystemPathError::PROTECTED_LOCATION);

    std::cout << "Passed: " << passed << " Failed: " << failed << '\n';
    return failed == 0 ? 0 : 1;
}
