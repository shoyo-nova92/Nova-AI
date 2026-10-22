#include "filesystem_location_resolver.hpp"

#include <windows.h>
#include <shlobj.h>

#include <algorithm>
#include <cctype>
#include <map>
#include <set>
#include <system_error>

namespace fs = std::filesystem;

namespace {
constexpr std::size_t kMaximumEntriesPerRoot = 50000;
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
        "place", "please", "put", "that", "the", "there", "this", "to", "under", "with"
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
    return lower(left.lexically_normal().string()) == lower(right.lexically_normal().string());
}

fs::path knownFolder(REFKNOWNFOLDERID id) {
    PWSTR value = nullptr;
    if (FAILED(SHGetKnownFolderPath(id, 0, nullptr, &value)) || value == nullptr) return {};
    fs::path result(value);
    CoTaskMemFree(value);
    return result;
}

int scoreDirectory(const fs::path& directory, const std::vector<std::string>& clues) {
    const auto nameWords = words(directory.filename().string());
    const auto pathWords = words(directory.string());
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
    for (std::size_t rootIndex = 0; rootIndex < roots.size(); ++rootIndex) {
        // The final root is the broad user-profile fallback. Once a known user
        // location produced candidates, scanning a huge redirected profile can
        // only slow the command down and turn a useful match into an incomplete
        // search result.
        if (rootIndex + 1 == roots.size() && !matches.empty()) break;
        const auto& rawRoot = roots[rootIndex];
        std::error_code error;
        if (rawRoot.empty() || !fs::is_directory(rawRoot, error)) continue;
        const fs::path root = fs::weakly_canonical(rawRoot, error);
        if (error) continue;
        const int rootScore = scoreDirectory(root, clueWords);
        if (rootScore > 0) matches[root.string()] = {root, rootScore};

        std::size_t visited = 0;
        fs::recursive_directory_iterator iterator(root, fs::directory_options::skip_permission_denied, error);
        const fs::recursive_directory_iterator end;
        while (!error && iterator != end) {
            if (++visited > kMaximumEntriesPerRoot) { incomplete = true; break; }
            const fs::directory_entry entry = *iterator;
            if (entry.is_directory(error) && !error) {
                const int score = scoreDirectory(entry.path(), clueWords);
                if (score > 0) {
                    const fs::path canonical = fs::weakly_canonical(entry.path(), error);
                    if (!error) matches[canonical.string()] = {canonical, score};
                    error.clear();
                }
            }
            iterator.increment(error);
        }
        if (error) { incomplete = true; error.clear(); }
    }

    for (const auto& [_, candidate] : matches) result.candidates.push_back(candidate);
    std::sort(result.candidates.begin(), result.candidates.end(), [](const auto& left, const auto& right) {
        if (left.score != right.score) return left.score > right.score;
        return lower(left.path.string()) < lower(right.path.string());
    });
    if (result.candidates.size() > kMaximumCandidates) result.candidates.resize(kMaximumCandidates);

    if (result.candidates.empty()) {
        result.status = incomplete ? FilesystemLocationResolution::INCOMPLETE : FilesystemLocationResolution::NOT_FOUND;
        result.message = incomplete ? "Location search reached its bounded scan limit before finding a match."
                                    : "No accessible directory matches the supplied location keywords.";
        return result;
    }
    const auto& best = result.candidates.front();
    if (result.candidates.size() > 1 && result.candidates[1].score == best.score) {
        result.status = FilesystemLocationResolution::AMBIGUOUS;
        result.message = "More than one directory matches the supplied location keywords.";
        return result;
    }
    result.status = FilesystemLocationResolution::RESOLVED;
    result.path = best.path;
    result.message = "Resolved directory from keyword search: " + best.path.string();
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
    // Profile is deliberately last: Desktop/Documents/Downloads are fast,
    // focused roots, while Profile is the fallback for arbitrary user folders.
    const fs::path profile = knownFolder(FOLDERID_Profile);
    if (!profile.empty() && std::none_of(roots.begin(), roots.end(), [&](const fs::path& root) {
            return samePath(profile, root);
        })) {
        roots.push_back(profile);
    }
    return roots;
}
