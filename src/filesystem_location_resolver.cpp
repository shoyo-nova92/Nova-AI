#include "filesystem_location_resolver.hpp"
#include "utf8_text.hpp"

#include <windows.h>
#include <shlobj.h>

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <cctype>
#include <map>
#include <set>
#include <system_error>

namespace fs = std::filesystem;

namespace {
constexpr std::size_t kMaximumEntriesPerRoot = 100000;
constexpr std::size_t kMaximumCandidates = 32;

std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

std::vector<std::string> words(const std::string& value) {
    std::vector<std::string> result;
    std::string word;
    for (unsigned char c : value) {
        if (std::isalnum(c)) word.push_back(static_cast<char>(std::tolower(c)));
        else if (!word.empty()) { result.push_back(word); word.clear(); }
    }
    if (!word.empty()) result.push_back(word);
    return result;
}

std::vector<std::string> meaningfulWords(const std::string& value) {
    static const std::set<std::string> ignored = {
        "a", "an", "and", "at", "directory", "existing", "find", "folder", "for",
        "from", "in", "inside", "into", "it", "location", "my", "of", "on", "path",
        "place", "please", "put", "that", "the", "there", "this", "to", "under", "with",
        "desktop", "documents", "downloads"
    };
    std::vector<std::string> result;
    for (const auto& word : words(value)) if (!ignored.contains(word)) result.push_back(word);
    return result;
}

bool wordMatches(const std::string& clue, const std::string& candidate) {
    if (clue == candidate) return true;
    // A cautious prefix match supports "sem" for "semester" without a broad
    // edit-distance guess that could direct a write to the wrong folder.
    return clue.size() >= 3 && candidate.size() >= 3 &&
        (candidate.starts_with(clue) || clue.starts_with(candidate));
}

bool pathContains(const fs::path& child, const fs::path& parent) {
    auto childIt = child.begin();
    for (auto parentIt = parent.begin(); parentIt != parent.end(); ++parentIt, ++childIt) {
        if (childIt == child.end() || lower(childIt->string()) != lower(parentIt->string())) return false;
    }
    return true;
}

bool samePath(const fs::path& left, const fs::path& right) {
    return lower(Utf8::path(left.lexically_normal())) == lower(Utf8::path(right.lexically_normal()));
}

fs::path queryKnownFolder(REFKNOWNFOLDERID id) {
    PWSTR value = nullptr;
    if (FAILED(SHGetKnownFolderPath(id, 0, nullptr, &value)) || value == nullptr) return {};
    fs::path result(value);
    CoTaskMemFree(value);
    return result;
}

fs::path knownFolder(REFKNOWNFOLDERID id) {
    // Keep intentional Windows redirection when the configured location exists.
    // If it points to a removed sync location, use the standard folder under the
    // current profile rather than carrying a stale path into every operation.
    const fs::path configured = queryKnownFolder(id);
    std::error_code error;
    if (!configured.empty() && fs::is_directory(configured, error) && !error)
        return configured;

    const fs::path profile = queryKnownFolder(FOLDERID_Profile);
    if (profile.empty()) return configured;

    fs::path fallback;
    if (IsEqualGUID(id, FOLDERID_Desktop)) fallback = profile / L"Desktop";
    else if (IsEqualGUID(id, FOLDERID_Documents)) fallback = profile / L"Documents";
    else if (IsEqualGUID(id, FOLDERID_Downloads)) fallback = profile / L"Downloads";
    else return configured;

    error.clear();
    if (fs::is_directory(fallback, error) && !error) return fallback;
    return configured.empty() ? fallback : configured;
}

bool skipDirectoryTree(const fs::path& path) {
    const std::string name = lower(Utf8::path(path.filename()));
    static const std::set<std::string> skipped = {
        "$recycle.bin", "appdata", "application data",
        "config.msi", "program files", "program files (x86)", "programdata",
        "recovery", "system volume information", "windows", "windows.old",
        "windowsapps"
    };
    return skipped.contains(name);
}

int scoreDirectory(const fs::path& directory, const std::vector<std::string>& clues) {
    const auto nameWords = words(Utf8::path(directory.filename()));
    const auto pathWords = words(Utf8::path(directory));
    int score = 0;
    for (const auto& clue : clues) {
        const bool nameMatch = std::any_of(nameWords.begin(), nameWords.end(), [&](const std::string& word) {
            return wordMatches(clue, word);
        });
        const bool pathMatch = std::any_of(pathWords.begin(), pathWords.end(), [&](const std::string& word) {
            return wordMatches(clue, word);
        });
        if (!pathMatch) return 0;
        score += nameMatch ? 50 : 15;
    }

    // Preserve the user's path-segment order as a ranking signal. It is not a
    // hard requirement because natural language may mention ancestors out of order.
    std::size_t cursor = 0;
    bool ordered = true;
    for (const auto& clue : clues) {
        auto match = pathWords.end();
        for (auto it = pathWords.begin() + static_cast<std::ptrdiff_t>(
                 std::min(cursor, pathWords.size())); it != pathWords.end(); ++it) {
            if (wordMatches(clue, *it)) { match = it; break; }
        }
        if (match == pathWords.end()) { ordered = false; break; }
        cursor = static_cast<std::size_t>(std::distance(pathWords.begin(), match)) + 1;
    }
    if (ordered) score += 25;
    return score;
}
} // namespace

FilesystemLocationResult FilesystemLocationResolver::resolve(const std::string& clues) const {
    return resolve(clues, discoveryRoots());
}

FilesystemLocationResult FilesystemLocationResolver::resolve(
    const std::string& clues, const std::vector<fs::path>& roots) const {
    FilesystemLocationResult result;
    const auto clueWords = meaningfulWords(clues);
    if (clueWords.empty()) {
        result.message = "Location search needs one or more folder keywords.";
        return result;
    }

    std::map<std::string, FilesystemLocationCandidate> matches;
    bool incomplete = false;
    const fs::path profile = knownFolder(FOLDERID_Profile);
    const fs::path desktopRootRaw = knownFolder(FOLDERID_Desktop);
    std::error_code desktopRootError;
    const fs::path desktopRoot = desktopRootRaw.empty()
        ? fs::path{} : fs::weakly_canonical(desktopRootRaw, desktopRootError);
    const auto rawWords = words(clues);
    const bool desktopRequested = std::any_of(rawWords.begin(), rawWords.end(),
        [](const std::string& clue) { return wordMatches(clue, "desktop"); });
    for (std::size_t rootIndex = 0; rootIndex < roots.size(); ++rootIndex) {
        const auto& rawRoot = roots[rootIndex];
        // Search user-known roots first, then expand across accessible volumes
        // if those roots do not contain a usable match.
        std::error_code error;
        if (rawRoot.empty() || !fs::is_directory(rawRoot, error)) continue;
        const fs::path root = fs::weakly_canonical(rawRoot, error);
        if (error) continue;
        const bool scanningVolumeRoot = (root == root.root_path());
        const int rootScore = scoreDirectory(root, clueWords);
        if (rootScore > 0) matches[Utf8::path(root)] = {root, rootScore};

        std::size_t visited = 0;
        fs::recursive_directory_iterator iterator(root, fs::directory_options::skip_permission_denied, error);
        const fs::recursive_directory_iterator end;
        while (!error && iterator != end) {
            if (++visited > kMaximumEntriesPerRoot) { incomplete = true; break; }
            const fs::directory_entry entry = *iterator;
            std::error_code entryError;
            const bool isSymlink = entry.is_symlink(entryError);
            const bool isDirectory = !entryError && entry.is_directory(entryError) && !entryError;
            if (isSymlink) {
                iterator.disable_recursion_pending();
            } else if (isDirectory) {
                const fs::path entryPath = entry.path();
                // Avoid enormous OS/application caches and do not scan the
                // current profile twice when a volume-wide fallback is needed.
                if (skipDirectoryTree(entryPath) ||
                    (scanningVolumeRoot && !profile.empty() && samePath(entryPath, profile))) {
                    iterator.disable_recursion_pending();
                } else {
                    const int score = scoreDirectory(entryPath, clueWords);
                    if (score > 0) {
                        const fs::path canonical = fs::weakly_canonical(entryPath, error);
                        if (!error) matches[Utf8::path(canonical)] = {canonical, score};
                        error.clear();
                    }
                }
            }
            iterator.increment(error);
        }
        if (error) { incomplete = true; error.clear(); }

        // When the user explicitly anchors the location at Desktop, a complete
        // search of the live Desktop tree is the most relevant search scope.
        // Do not let an unrelated, much larger profile scan override it.
        if (desktopRequested && !matches.empty() && !desktopRoot.empty() &&
            !desktopRootError) {
            const bool scanningDesktop = samePath(root, desktopRoot);
            const bool scanningProfile = !profile.empty() && samePath(root, profile);
            const bool hasDesktopCandidate = std::any_of(matches.begin(), matches.end(),
                [](const auto& item) {
                    const auto pathWords = words(Utf8::path(item.second.path));
                    return std::any_of(pathWords.begin(), pathWords.end(),
                        [](const std::string& word) { return wordMatches(word, "desktop"); });
                });
            if (scanningDesktop || (scanningProfile && hasDesktopCandidate)) break;
        }
    }

    for (const auto& [_, candidate] : matches) result.candidates.push_back(candidate);
    std::sort(result.candidates.begin(), result.candidates.end(), [](const auto& left, const auto& right) {
        if (left.score != right.score) return left.score > right.score;
        return lower(Utf8::path(left.path)) < lower(Utf8::path(right.path));
    });
    if (result.candidates.size() > kMaximumCandidates) result.candidates.resize(kMaximumCandidates);

    if (incomplete) {
        result.status = FilesystemLocationResolution::INCOMPLETE;
        result.message = "Location search was incomplete; Nova cannot safely claim a unique destination.";
        return result;
    }
    if (result.candidates.empty()) {
        result.status = FilesystemLocationResolution::NOT_FOUND;
        result.message = "No accessible directory matches the supplied location keywords.";
        return result;
    }
    const auto& best = result.candidates.front();
    if (best.score < static_cast<int>(clueWords.size() * 15)) {
        result.status = FilesystemLocationResolution::NOT_FOUND;
        result.message = "No candidate met the minimum keyword-match threshold.";
        return result;
    }
    if (result.candidates.size() > 1 &&
        (result.candidates[1].score == best.score ||
         best.score - result.candidates[1].score < 20)) {
        result.status = FilesystemLocationResolution::AMBIGUOUS;
        result.message = "Multiple directories have similarly strong matches for the supplied location keywords.";
        return result;
    }
    result.status = FilesystemLocationResolution::RESOLVED;
    result.path = best.path;
    result.message = "Resolved directory from keyword search: " + Utf8::path(best.path);
    return result;
}

std::vector<fs::path> FilesystemLocationResolver::discoveryRoots() const {
    std::vector<fs::path> roots;
    for (REFKNOWNFOLDERID id : {FOLDERID_Desktop, FOLDERID_Documents, FOLDERID_Downloads}) {
        const fs::path folder = knownFolder(id);
        if (!folder.empty() && std::none_of(roots.begin(), roots.end(), [&](const fs::path& root) {
                return samePath(folder, root);
            })) roots.push_back(folder);
    }
    // Search the user profile after the known folders. If no match is found
    // there, expand to accessible fixed/removable volumes instead of assuming
    // all user-created directories live under the profile.
    const fs::path profile = knownFolder(FOLDERID_Profile);
    if (!profile.empty() && std::none_of(roots.begin(), roots.end(), [&](const fs::path& root) {
            return samePath(profile, root);
        })) {
        roots.push_back(profile);
    }

    const DWORD drives = GetLogicalDrives();
    for (unsigned index = 0; index < 26; ++index) {
        if ((drives & (1u << index)) == 0) continue;
        wchar_t rootBuffer[] = L"A:\\";
        rootBuffer[0] = static_cast<wchar_t>(L'A' + index);
        const UINT driveType = GetDriveTypeW(rootBuffer);
        if (driveType != DRIVE_FIXED && driveType != DRIVE_REMOVABLE) continue;
        const fs::path driveRoot(rootBuffer);
        if (std::none_of(roots.begin(), roots.end(), [&](const fs::path& root) {
                return samePath(driveRoot, root);
            })) roots.push_back(driveRoot);
    }
    return roots;
}
