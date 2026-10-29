#include "filesystem_ranking.hpp"
#include <windows.h>
#include <winhttp.h>
#include <nlohmann/json.hpp>
#include <psapi.h>
#include <shlobj.h>
#include <shobjidl.h>
#include "primitive_execution_engine.hpp"
#include "utf8_text.hpp"
#include "environment_snapshot.hpp"
#include "application_observation.hpp"
#include "filesystem_path_policy.hpp"
#include "filesystem_location_resolver.hpp"

#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>
#include <algorithm>
#include <cctype>
#include <sstream>
#include <regex>
#include <set>
#include <array>
#include <cstring>
#include <iomanip>
#include <ctime>
#include "application_resolver.hpp"

using namespace std;
namespace fs = std::filesystem;

namespace {
ExecutionResult makeResult(const PrimitiveOperation& op, ExecutionStatus status,
    ExecutionFailureReason reason, const string& message,
    bool targetResolved = false, bool mechanismResolved = false) {
    ExecutionResult r{};
    r.status = status;
    r.reason = reason;
    r.action = op.name;
    r.message = message;
    switch (reason) {
        case ExecutionFailureReason::NONE: r.failureCategory = "none"; break;
        case ExecutionFailureReason::INVALID_PLAN: r.failureCategory = "invalid_plan"; break;
        case ExecutionFailureReason::LOW_CONFIDENCE: r.failureCategory = "low_confidence"; break;
        case ExecutionFailureReason::UNSUPPORTED_ACTION: r.failureCategory = "unsupported_action"; break;
        case ExecutionFailureReason::TARGET_NOT_FOUND: r.failureCategory = "target_not_found"; break;
        case ExecutionFailureReason::EXECUTION_ERROR: r.failureCategory = "execution_error"; break;
    }
    r.confidence = targetResolved && mechanismResolved ? 1.0 : 0.0;
    r.targetResolved = targetResolved;
    r.executionMechanismResolved = mechanismResolved;
    if(op.name.starts_with("application."))
        r.factsJson=Utf8::dump(nlohmann::json{{"category","APPLICATION_DISPATCH"},{"dispatch_accepted",status==ExecutionStatus::SUCCESS},{"target_resolved",targetResolved},{"mechanism_resolved",mechanismResolved}},"application.facts");
    return r;
}

ExecutionResult fail(const PrimitiveOperation& op, ExecutionFailureReason reason,
    const string& message, bool valid = true) {
    return makeResult(op, valid ? ExecutionStatus::FAILED : ExecutionStatus::NOT_EXECUTED,
        reason, message, valid, valid);
}

bool leafName(const string& name) {
    if (name.empty() || name == "." || name == ".." ||
        name.find('/') != string::npos || name.find('\\') != string::npos) return false;
    fs::path p(name);
    return !p.is_absolute() && !p.has_root_name() && !p.has_root_directory() && p.filename() == p;
}

ExecutionFailureReason failureReasonForPath(FilesystemPathError error) {
    switch (error) {
        case FilesystemPathError::NOT_FOUND: return ExecutionFailureReason::TARGET_NOT_FOUND;
        case FilesystemPathError::ACCESS_DENIED:
        case FilesystemPathError::IO_ERROR: return ExecutionFailureReason::EXECUTION_ERROR;
        case FilesystemPathError::NONE:
        case FilesystemPathError::MALFORMED_PATH:
        case FilesystemPathError::UNKNOWN_LOCATION:
        case FilesystemPathError::AMBIGUOUS_LOCATION:
        case FilesystemPathError::PROTECTED_LOCATION: return ExecutionFailureReason::UNSUPPORTED_ACTION;
    }
    return ExecutionFailureReason::UNSUPPORTED_ACTION;
}

bool resolvePathForOperation(const PrimitiveOperation& op, const string& raw,
                             FilesystemAccess access, fs::path& resolved,
                             ExecutionResult& failure, const fs::path& explicitParent = {}) {
    FilesystemPathPolicy policy;
    const FilesystemPathResult path = policy.resolve(raw, explicitParent);

    const bool expectsDirectory =
        op.type == PrimitiveOperationType::FILESYSTEM_CREATE ||
        op.type == PrimitiveOperationType::FILESYSTEM_CREATE_FILE ||
        op.type == PrimitiveOperationType::FILESYSTEM_LIST_DIRECTORY ||
        op.type == PrimitiveOperationType::FILESYSTEM_SEARCH;

    std::error_code locationExistsError;
    const bool locationExists = path.ok && fs::exists(path.path, locationExistsError);
    const bool resolvedDirectoryMissing =
        path.ok && expectsDirectory && !locationExists && !locationExistsError;

    auto resolveByKeywords = [&]() -> bool {
        const FilesystemLocationResult discovery = FilesystemLocationResolver{}.resolve(raw);
        if (discovery.status == FilesystemLocationResolution::RESOLVED) {
            const FilesystemPathResult authorization = policy.authorize(discovery.path, access);
            if (authorization.ok) {
                resolved = authorization.path;
                return true;
            }
            failure = fail(op, failureReasonForPath(authorization.error),
                "Filesystem location was found but is not authorized (" +
                FilesystemPathPolicy::errorName(authorization.error) + "): " + authorization.message,
                false);
            failure.failureCategory = FilesystemPathPolicy::errorName(authorization.error);
            failure.resolvedPath = Utf8::path(authorization.path);
            return false;
        }

        FilesystemPathError discoveryError = FilesystemPathError::NOT_FOUND;
        if (discovery.status == FilesystemLocationResolution::AMBIGUOUS)
            discoveryError = FilesystemPathError::AMBIGUOUS_LOCATION;
        else if (discovery.status == FilesystemLocationResolution::INCOMPLETE)
            discoveryError = FilesystemPathError::IO_ERROR;

        failure = fail(op, failureReasonForPath(discoveryError),
            "Filesystem keyword location search failed (" +
            (discovery.status == FilesystemLocationResolution::INCOMPLETE
                ? string("incomplete_search")
                : FilesystemPathPolicy::errorName(discoveryError)) +
            "): " + discovery.message, false);
        failure.failureCategory = discovery.status == FilesystemLocationResolution::INCOMPLETE
            ? "incomplete_search" : FilesystemPathPolicy::errorName(discoveryError);
        for (const auto& candidate : discovery.candidates)
            failure.discoveredPaths.push_back(Utf8::path(candidate.path));
        return false;
    };

    if (!path.ok) {
        if (path.error == FilesystemPathError::UNKNOWN_LOCATION)
            return resolveByKeywords();

        failure = fail(op, failureReasonForPath(path.error),
            "Filesystem path resolution failed (" + FilesystemPathPolicy::errorName(path.error) + "): " + path.message,
            false);
        failure.failureCategory = FilesystemPathPolicy::errorName(path.error);
        return false;
    }

    // A syntactically valid alias can still point at a stale or nonexistent
    // directory (for example, a Desktop redirected to a removed sync folder).
    // For directory-location operations, search for the existing destination
    // before allowing the planner to treat that missing path as a new parent.
    if (resolvedDirectoryMissing)
        return resolveByKeywords();

    const FilesystemPathResult authorization = policy.authorize(path.path, access);
    if (!authorization.ok) {
        failure = fail(op, failureReasonForPath(authorization.error),
            "Filesystem path authorization failed (" + FilesystemPathPolicy::errorName(authorization.error) + "): " + authorization.message,
            false);
        failure.resolvedPath = Utf8::path(authorization.path);
        failure.failureCategory = FilesystemPathPolicy::errorName(authorization.error);
        return false;
    }
    resolved = authorization.path;
    return true;
}

ExecutionResult filesystemSuccess(const PrimitiveOperation& op, const string& message,
                                  const fs::path& path,
                                  vector<string> discovered = {}) {
    ExecutionResult result = makeResult(op, ExecutionStatus::SUCCESS,
        ExecutionFailureReason::NONE, message, true, true);
    result.resolvedPath = Utf8::path(path);
    result.discoveredPaths = move(discovered);
    nlohmann::json facts={{"path",result.resolvedPath},{"entry_count",result.discoveredPaths.size()}};
    error_code error;
    const bool exists=fs::exists(path,error);
    if (!error) {
        facts["exists"]=exists;
        if (exists) {
            facts["type"]=fs::is_directory(path)?"directory":fs::is_regular_file(path)?"file":"other";
            if (fs::is_regular_file(path)) facts["bytes"]=fs::file_size(path);
        } else facts["type"]="missing";
    }
    result.factsJson=Utf8::dump(facts,"filesystem.facts");
    return result;
}
bool identicalFileBytes(const fs::path& source,const fs::path& destination) {
    ifstream a(source,ios::binary),b(destination,ios::binary);
    if (!a || !b) return false;
    array<char,65536> first{},second{};
    do {
        a.read(first.data(),first.size()); b.read(second.data(),second.size());
        if (a.gcount()!=b.gcount() || memcmp(first.data(),second.data(),static_cast<size_t>(a.gcount()))!=0) return false;
    } while (a.gcount());
    return !a.bad() && !b.bad();
}

bool boolParameter(const string& value, bool defaultValue, bool& out) {
    if (value.empty()) { out = defaultValue; return true; }
    if (value == "true" || value == "1") { out = true; return true; }
    if (value == "false" || value == "0") { out = false; return true; }
    return false;
}

string lower(string s) {
    transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(tolower(c));
    });
    return s;
}

bool sameProcessImage(DWORD pid, const string& expected) {
    return !expected.empty() && lower(processImageName(pid))==lower(expected);
}

struct WindowSearch {
    DWORD pid = 0;
    HWND hwnd = nullptr;
    string titleNeedle;
};

BOOL CALLBACK enumWindowsProc(HWND hwnd, LPARAM lParam) {
    auto* search = reinterpret_cast<WindowSearch*>(lParam);
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid != search->pid || !IsWindowVisible(hwnd)) return TRUE;

    wchar_t title[1024]{};
    GetWindowTextW(hwnd, title, static_cast<int>(std::size(title)));
    string titleText=Utf8::path(fs::path(title));
    if (!search->titleNeedle.empty() &&
        lower(titleText).find(lower(search->titleNeedle)) == string::npos) return TRUE;

    search->hwnd = hwnd;
    return FALSE;
}

HWND findWindowForPid(DWORD pid, const string& titleNeedle = {}) {
    WindowSearch search{pid, nullptr, titleNeedle};
    EnumWindows(enumWindowsProc, reinterpret_cast<LPARAM>(&search));
    return search.hwnd;
}

string appExeFromName(const string& application) {
    const auto requested=lower(application);
    string match;
    const auto inventory=environmentSnapshot();
    for (const auto& item:inventory["visible_applications"]) {
        const auto image=item.at("application").get<string>();
        if (lower(image)==requested || lower(Utf8::path(Utf8::toPath(image).stem()))==requested) {
            if (!match.empty() && lower(match)!=lower(image)) return {};
            match=image;
        }
    }
    return match;
}

bool launchTargetIsSafe(const string& target, fs::path& resolved) {
    if (target.empty()) return true;
    FilesystemPathPolicy policy;
    const FilesystemPathResult path = policy.resolve(target);
    if (!path.ok) return false;
    const FilesystemPathResult allowed = policy.authorize(path.path, FilesystemAccess::DISCOVER);
    if (!allowed.ok) return false;
    resolved = allowed.path;
    return fs::exists(resolved);
}

bool focusWindowByTitle(const string& titleNeedle, HWND& found) {
    found = nullptr;
    if (titleNeedle.empty()) return true; // use the current foreground input target
    struct Search { string needle; HWND hwnd; } search{titleNeedle, nullptr};
    auto callback = [](HWND hwnd, LPARAM param) -> BOOL {
        auto* s = reinterpret_cast<Search*>(param);
        if (!IsWindowVisible(hwnd)) return TRUE;
        wchar_t title[1024]{};
        GetWindowTextW(hwnd, title, static_cast<int>(std::size(title)));
        string value=Utf8::path(fs::path(title));
        string needle = s->needle;
        transform(value.begin(), value.end(), value.begin(),
            [](unsigned char c) { return static_cast<char>(tolower(c)); });
        transform(needle.begin(), needle.end(), needle.begin(),
            [](unsigned char c) { return static_cast<char>(tolower(c)); });
        if (!needle.empty() && value.find(needle) != string::npos) {
            s->hwnd = hwnd;
            return FALSE;
        }
        return TRUE;
    };
    EnumWindows(callback, reinterpret_cast<LPARAM>(&search));
    found = search.hwnd;
    if (!found) return false;
    ShowWindow(found, SW_RESTORE);
    SetForegroundWindow(found);
    SetFocus(found);
    return true;
}

bool sendUtf8Text(const string& text, string& error) {
    if (text.empty()) return true;
    const int wideLength = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
        text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (wideLength <= 0) {
        error = "Text is not valid UTF-8.";
        return false;
    }
    vector<wchar_t> wide(static_cast<size_t>(wideLength));
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
        text.data(), static_cast<int>(text.size()), wide.data(), wideLength) != wideLength) {
        error = "UTF-8 to UTF-16 conversion failed.";
        return false;
    }

    vector<INPUT> inputs;
    inputs.reserve(wide.size() * 2);
    for (wchar_t ch : wide) {
        INPUT down{};
        down.type = INPUT_KEYBOARD;
        down.ki.wScan = static_cast<WORD>(ch);
        down.ki.dwFlags = KEYEVENTF_UNICODE;
        inputs.push_back(down);
        INPUT up = down;
        up.ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
        inputs.push_back(up);
    }
    const UINT sent = SendInput(static_cast<UINT>(inputs.size()), inputs.data(), sizeof(INPUT));
    if (sent != inputs.size()) {
        error = "SendInput accepted " + to_string(sent) + " of " +
            to_string(inputs.size()) + " keyboard events.";
        return false;
    }
    return true;
}

WORD virtualKeyFor(const string& key) {
    string k = key;
    transform(k.begin(), k.end(), k.begin(),
        [](unsigned char c) { return static_cast<char>(toupper(c)); });
    if (k.size() == 1) {
        if ((k[0] >= 'A' && k[0] <= 'Z') || (k[0] >= '0' && k[0] <= '9'))
            return static_cast<WORD>(k[0]);
    }
    if (k.size() >= 2 && k[0] == 'F') {
        try {
            int n = stoi(k.substr(1));
            if (n >= 1 && n <= 24) return static_cast<WORD>(VK_F1 + n - 1);
        } catch (...) {}
    }
    if (k == "ENTER" || k == "RETURN") return VK_RETURN;
    if (k == "TAB") return VK_TAB;
    if (k == "ESC" || k == "ESCAPE") return VK_ESCAPE;
    if (k == "SPACE" || k == "SPACEBAR") return VK_SPACE;
    if (k == "BACKSPACE") return VK_BACK;
    if (k == "DELETE" || k == "DEL") return VK_DELETE;
    if (k == "INSERT" || k == "INS") return VK_INSERT;
    if (k == "HOME") return VK_HOME;
    if (k == "END") return VK_END;
    if (k == "PAGEUP" || k == "PAGE UP") return VK_PRIOR;
    if (k == "PAGEDOWN" || k == "PAGE DOWN") return VK_NEXT;
    if (k == "UP" || k == "ARROWUP") return VK_UP;
    if (k == "DOWN" || k == "ARROWDOWN") return VK_DOWN;
    if (k == "LEFT" || k == "ARROWLEFT") return VK_LEFT;
    if (k == "RIGHT" || k == "ARROWRIGHT") return VK_RIGHT;
    if (k == "CTRL" || k == "CONTROL") return VK_CONTROL;
    if (k == "ALT") return VK_MENU;
    if (k == "SHIFT") return VK_SHIFT;
    if (k == "WIN" || k == "WINDOWS") return VK_LWIN;
    return 0;
}

bool parseModifiers(const string& text, vector<WORD>& modifiers) {
    modifiers.clear();
    if (text.empty()) return true;
    string token;
    stringstream stream(text);
    while (getline(stream, token, ',')) {
        token.erase(0, token.find_first_not_of(" \t"));
        const size_t last = token.find_last_not_of(" \t");
        if (last == string::npos) return false;
        token.erase(last + 1);
        const WORD vk = virtualKeyFor(token);
        if (vk != VK_CONTROL && vk != VK_MENU && vk != VK_SHIFT &&
            vk != VK_LWIN && vk != VK_RWIN) return false;
        modifiers.push_back(vk);
    }
    return !modifiers.empty();
}

bool sendKeyEvent(WORD vk, bool keyUp = false) {
    INPUT input{};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = vk;
    input.ki.dwFlags = keyUp ? KEYEVENTF_KEYUP : 0;
    return SendInput(1, &input, sizeof(INPUT)) == 1;
}

bool sendKeyCombo(WORD key, const vector<WORD>& modifiers, int repeatCount, string& error) {
    for (WORD modifier : modifiers) {
        if (!sendKeyEvent(modifier, false)) {
            error = "Failed to send modifier key-down event.";
            return false;
        }
    }
    for (int i = 0; i < repeatCount; ++i) {
        if (!sendKeyEvent(key, false) || !sendKeyEvent(key, true)) {
            error = "Failed to send key press event.";
            for (auto it = modifiers.rbegin(); it != modifiers.rend(); ++it) sendKeyEvent(*it, true);
            return false;
        }
    }
    for (auto it = modifiers.rbegin(); it != modifiers.rend(); ++it) {
        if (!sendKeyEvent(*it, true)) {
            error = "Failed to send modifier key-up event.";
            return false;
        }
    }
    return true;
}


bool parseCoordinate(const string& text, int& value) {
    if (text.empty()) return false;
    try {
        size_t used = 0;
        long parsed = stol(text, &used);
        if (used != text.size() || parsed < -100000 || parsed > 100000) return false;
        value = static_cast<int>(parsed);
        return true;
    } catch (...) {
        return false;
    }
}

bool parseClickCount(const string& text, int& value) {
    if (text.empty()) { value = 1; return true; }
    try {
        size_t used = 0;
        int parsed = stoi(text, &used);
        if (used != text.size() || parsed < 1 || parsed > 2) return false;
        value = parsed;
        return true;
    } catch (...) {
        return false;
    }
}

bool moveCursorTo(int x, int y, string& error) {
    const int left = GetSystemMetrics(SM_XVIRTUALSCREEN);
    const int top = GetSystemMetrics(SM_YVIRTUALSCREEN);
    const int width = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    const int height = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    if (width <= 0 || height <= 0 || x < left || y < top ||
        x >= left + width || y >= top + height) {
        error = "Coordinates are outside the virtual desktop bounds.";
        return false;
    }
    if (!SetCursorPos(x, y)) {
        error = "SetCursorPos failed with Windows error " + to_string(GetLastError()) + ".";
        return false;
    }
    POINT actual{};
    if (!GetCursorPos(&actual) || actual.x != x || actual.y != y) {
        error = "Cursor position verification failed.";
        return false;
    }
    return true;
}

bool validHttpUrl(const string& url, string& error) {
    if (url.empty() || url.size() > 8192) {
        error = "URL must be non-empty and no longer than 8192 characters.";
        return false;
    }
    for (unsigned char ch : url) {
        if (ch <= 0x20 || ch == 0x7f) {
            error = "URL must not contain whitespace or control characters.";
            return false;
        }
    }
    const size_t schemeEnd = url.find("://");
    if (schemeEnd == string::npos) {
        error = "Only absolute HTTP and HTTPS URLs are allowed.";
        return false;
    }
    const string scheme = lower(url.substr(0, schemeEnd));
    if (scheme != "http" && scheme != "https") {
        error = "Only absolute HTTP and HTTPS URLs are allowed.";
        return false;
    }
    const size_t authorityStart = schemeEnd + 3;
    const size_t authorityEnd = url.find_first_of("/?#", authorityStart);
    const string authority = url.substr(authorityStart,
        authorityEnd == string::npos ? string::npos : authorityEnd - authorityStart);
    if (authority.empty() || authority.find('@') != string::npos) {
        error = "URL must include a host and must not contain embedded credentials.";
        return false;
    }
    return true;
}

bool browserHttpRequest(const wchar_t* method, const wchar_t* path,
                        const string& body, string& response, DWORD& status) {
    HINTERNET session = WinHttpOpen(L"NovaBrowserControl/1.0",
        WINHTTP_ACCESS_TYPE_NO_PROXY, WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) return false;
    WinHttpSetTimeouts(session, 3000, 5000, 10000, 90000);
    HINTERNET connection = WinHttpConnect(session, L"127.0.0.1", 47621, 0);
    if (!connection) { WinHttpCloseHandle(session); return false; }
    HINTERNET request = WinHttpOpenRequest(connection, method, path, nullptr,
        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
    if (!request) {
        WinHttpCloseHandle(connection); WinHttpCloseHandle(session); return false;
    }
    const wchar_t* headers = L"Content-Type: application/json; charset=utf-8\r\n";
    const void* data = body.empty() ? WINHTTP_NO_REQUEST_DATA : body.data();
    const DWORD dataSize = static_cast<DWORD>(body.size());
    BOOL ok = WinHttpSendRequest(request, headers, static_cast<DWORD>(-1),
        const_cast<void*>(data), dataSize, dataSize, 0);
    if (ok) ok = WinHttpReceiveResponse(request, nullptr);
    if (ok) {
        DWORD size = sizeof(status);
        ok = WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX);
    }
    if (ok) {
        response.clear();
        DWORD available = 0;
        while (WinHttpQueryDataAvailable(request, &available) && available > 0) {
            string chunk(available, '\0');
            DWORD read = 0;
            if (!WinHttpReadData(request, chunk.data(), available, &read)) { ok = FALSE; break; }
            chunk.resize(read);
            response += chunk;
        }
    }
    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connection);
    WinHttpCloseHandle(session);
    return ok == TRUE;
}


bool ensureBrowserAgent(string& error) {
    string response;
    DWORD status = 0;

    if (browserHttpRequest(L"GET", L"/health", {}, response, status) &&
        status == 200 && response.find("NovaBrowserAgent") != string::npos) {
        return true;
    }

    wchar_t modulePath[MAX_PATH * 4]{};
    const DWORD length = GetModuleFileNameW(
        nullptr, modulePath, static_cast<DWORD>(std::size(modulePath)));

    if (length == 0 || length >= std::size(modulePath)) {
        error = "Cannot locate Nova executable to start the browser agent.";
        return false;
    }

    const fs::path root =
        fs::path(wstring(modulePath, length)).parent_path().parent_path();

    const fs::path script = root / "python" / "browser_agent.py";
    const fs::path venvPython = root / ".venv-win" / "Scripts" / "python.exe";

    if (!fs::exists(script)) {
        error = "Browser agent script not found: " + Utf8::path(script);
        return false;
    }

    if (!fs::exists(venvPython)) {
        error = "Nova virtual-environment Python not found: " +
                Utf8::path(venvPython);
        return false;
    }

    // Quote executable and script paths for Windows process creation.
    wstring command=L"\""+venvPython.native()+L"\" \""+script.native()+L"\"";
    vector<wchar_t> mutableCommand(command.begin(),command.end());
    mutableCommand.push_back(L'\0');

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};

    if (!CreateProcessW(
            venvPython.c_str(),
            mutableCommand.data(),
            nullptr, nullptr, FALSE,
            DETACHED_PROCESS | CREATE_NO_WINDOW,
            nullptr, root.c_str(),
            &startup, &process)) {
        error = "Could not start browser agent with " +
                Utf8::path(venvPython) +
                " (Windows error " + to_string(GetLastError()) + ").";
        return false;
    }

    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);

    for (int i = 0; i < 40; ++i) {
        Sleep(250);

        if (browserHttpRequest(L"GET", L"/health", {}, response, status) &&
            status == 200 &&
            response.find("NovaBrowserAgent") != string::npos) {
            return true;
        }
    }

    error = "Browser agent did not become ready. Verify the Python "
            "environment and python/browser_agent.py.";
    return false;
}


bool browserAgentRequest(const nlohmann::json& payload, nlohmann::json& result, string& error) {
    if (!ensureBrowserAgent(error)) return false;
    string response;
    DWORD status = 0;
    if (!browserHttpRequest(L"POST", L"/command", Utf8::dump(payload,"browser.request"), response, status)) {
        error = "Could not communicate with the local browser agent.";
        return false;
    }
    try {
        result = nlohmann::json::parse(response);
    } catch (...) {
        error = "Browser agent returned invalid JSON.";
        return false;
    }
    if (status < 200 || status >= 300 || !result.value("success", false)) {
        error = result.value("error", string("Browser agent request failed with HTTP status ") + to_string(status) + ".");
        return false;
    }
    return true;
}

bool sendMouseClick(const string& button, int clickCount, string& error) {
    DWORD down = 0, up = 0;
    const string b = lower(button);
    if (b == "left" || b == "l") {
        down = MOUSEEVENTF_LEFTDOWN; up = MOUSEEVENTF_LEFTUP;
    } else if (b == "right" || b == "r") {
        down = MOUSEEVENTF_RIGHTDOWN; up = MOUSEEVENTF_RIGHTUP;
    } else if (b == "middle" || b == "m") {
        down = MOUSEEVENTF_MIDDLEDOWN; up = MOUSEEVENTF_MIDDLEUP;
    } else {
        error = "button must be left, right, or middle.";
        return false;
    }

    for (int i = 0; i < clickCount; ++i) {
        INPUT inputs[2]{};
        inputs[0].type = INPUT_MOUSE;
        inputs[0].mi.dwFlags = down;
        inputs[1].type = INPUT_MOUSE;
        inputs[1].mi.dwFlags = up;
        if (SendInput(2, inputs, sizeof(INPUT)) != 2) {
            error = "SendInput failed while clicking (Windows error " +
                to_string(GetLastError()) + ").";
            return false;
        }
        if (i + 1 < clickCount) Sleep(GetDoubleClickTime() / 2);
    }
    return true;
}

}

ExecutionResult PrimitiveExecutionEngine::execute(const PrimitiveOperation& op) {
    try { return executeValidated(op); }
    catch (const Utf8::Error& error) {
        auto result=makeResult(op,ExecutionStatus::NOT_EXECUTED,ExecutionFailureReason::INVALID_PLAN,error.what());
        result.failureCategory="invalid_utf8"; return result;
    }
    catch (const std::exception& error) {
        return makeResult(op,ExecutionStatus::FAILED,ExecutionFailureReason::EXECUTION_ERROR,error.what());
    }
}
ExecutionResult PrimitiveExecutionEngine::executeValidated(const PrimitiveOperation& op) {
    if (!op.resolved)
        return makeResult(op, ExecutionStatus::NOT_EXECUTED, ExecutionFailureReason::INVALID_PLAN,
            "Primitive operation is unresolved.");

    if (op.name=="environment.observe" && op.type==PrimitiveOperationType::ENVIRONMENT_OBSERVE)
        return makeResult(op,ExecutionStatus::SUCCESS,ExecutionFailureReason::NONE,
            "Environment: " + Utf8::dump(environmentSnapshot(),"environment"),true,true);

    if (op.type == PrimitiveOperationType::FILESYSTEM_CREATE &&
        op.name == "filesystem.create") {
        if (!leafName(op.parameters.name))
            return fail(op, ExecutionFailureReason::UNSUPPORTED_ACTION, "Invalid folder name.", false);
        fs::path parent; ExecutionResult pathFailure;
        if (!resolvePathForOperation(op, op.parameters.location, FilesystemAccess::CREATE, parent, pathFailure)) return pathFailure;

        try {
            if (!fs::is_directory(parent)) return fail(op, ExecutionFailureReason::TARGET_NOT_FOUND,
                "Parent directory does not exist: " + Utf8::path(parent));
            const fs::path folder = parent / Utf8::toPath(op.parameters.name);
            if (fs::exists(folder)) {
                const auto authorization = FilesystemPathPolicy{}.authorize(folder, FilesystemAccess::CREATE);
                if (!authorization.ok) {
                    auto denied = fail(op, failureReasonForPath(authorization.error), authorization.message, false);
                    denied.failureCategory = FilesystemPathPolicy::errorName(authorization.error);
                    denied.resolvedPath = Utf8::path(authorization.path);
                    return denied;
                }
                auto existing = fail(op, ExecutionFailureReason::EXECUTION_ERROR, "Destination already exists: " + Utf8::path(folder));
                existing.failureCategory = "already_exists";
                existing.resolvedPath = Utf8::path(folder);
                existing.postconditionVerified = fs::is_directory(folder);
                existing.verifiedKind = existing.postconditionVerified ? "directory_exists" : "";
                return existing;
            }
            if (!fs::create_directory(folder)) return fail(op, ExecutionFailureReason::EXECUTION_ERROR,
                "Folder could not be created: " + Utf8::path(folder));
            auto created = filesystemSuccess(op, "Folder created: " + Utf8::path(folder), folder);
            created.postconditionVerified = fs::is_directory(folder);
            created.verifiedKind = "directory_exists";
            return created;
        } catch (const fs::filesystem_error& e) {
            return fail(op, ExecutionFailureReason::EXECUTION_ERROR, e.what());
        }
    }

    if (op.type == PrimitiveOperationType::FILESYSTEM_CREATE_FILE && op.name == "filesystem.create_file") {
        if (!leafName(op.parameters.name)) return fail(op, ExecutionFailureReason::UNSUPPORTED_ACTION, "Invalid file name.", false);
        fs::path dir; ExecutionResult pathFailure;
        if (!resolvePathForOperation(op, op.parameters.location, FilesystemAccess::CREATE, dir, pathFailure)) return pathFailure;
        const fs::path filePath = dir / Utf8::toPath(op.parameters.name);
        try {
            if (!fs::is_directory(dir)) return fail(op, ExecutionFailureReason::TARGET_NOT_FOUND, "Parent directory does not exist: " + Utf8::path(dir));
            if (fs::exists(filePath)) {
                auto collision = fail(op, ExecutionFailureReason::EXECUTION_ERROR, "File already exists: " + Utf8::path(filePath));
                collision.failureCategory = "already_exists";
                collision.resolvedPath = Utf8::path(filePath);
                return collision;
            }
            ofstream file(filePath, ios::binary);
            if (!file.is_open()) return fail(op, ExecutionFailureReason::EXECUTION_ERROR, "Unable to create file: " + Utf8::path(filePath));
            file.write(op.parameters.content.data(), static_cast<streamsize>(op.parameters.content.size()));
            file.close();
            if (!file) { error_code ignored; fs::remove(filePath, ignored); return fail(op, ExecutionFailureReason::EXECUTION_ERROR, "Failed writing file: " + Utf8::path(filePath)); }
            ifstream verified(filePath, ios::binary);
            const string actual((istreambuf_iterator<char>(verified)), istreambuf_iterator<char>());
            if (!verified.is_open() || verified.bad() || !fs::is_regular_file(filePath) || actual != op.parameters.content)
                return fail(op, ExecutionFailureReason::EXECUTION_ERROR, "Created file failed content readback verification: " + Utf8::path(filePath));
            auto created = filesystemSuccess(op, "File created and content verified: " + Utf8::path(filePath), filePath);
            created.postconditionVerified = true;
            created.verifiedKind = "file_content";
            return created;
        } catch (const fs::filesystem_error& e) { return fail(op, ExecutionFailureReason::EXECUTION_ERROR, e.what()); }
    }

    if (op.type == PrimitiveOperationType::FILESYSTEM_COPY && op.name == "filesystem.copy") {
        fs::path source, dest; ExecutionResult pathFailure;
        if (!resolvePathForOperation(op, op.parameters.source, FilesystemAccess::DISCOVER, source, pathFailure) ||
            !resolvePathForOperation(op, op.parameters.destination, FilesystemAccess::COPY, dest, pathFailure, source.parent_path())) return pathFailure;
        bool overwrite = false;
        if (!boolParameter(op.parameters.overwrite, false, overwrite))
            return fail(op, ExecutionFailureReason::INVALID_PLAN, "overwrite must be true/false or 1/0.", false);
        try {
            if (!fs::exists(source)) return fail(op, ExecutionFailureReason::TARGET_NOT_FOUND, "Copy source does not exist: " + Utf8::path(source));
            if (fs::exists(dest) && !overwrite) return fail(op, ExecutionFailureReason::EXECUTION_ERROR, "Copy destination already exists.");
            if (!fs::is_directory(dest.parent_path())) return fail(op, ExecutionFailureReason::TARGET_NOT_FOUND,
                "Copy destination parent does not exist: " + Utf8::path(dest.parent_path()) + " | resolved_destination=" + Utf8::path(dest) + " | resolved_source=" + Utf8::path(source));
            if (fs::is_directory(source)) {
                if (fs::exists(dest)) return fail(op, ExecutionFailureReason::UNSUPPORTED_ACTION, "Directory copy to an existing destination is unsupported.");
                fs::copy(source, dest, fs::copy_options::recursive);
            } else if (overwrite) fs::copy_file(source, dest, fs::copy_options::overwrite_existing);
            else fs::copy_file(source, dest);
            const bool directory=fs::is_directory(source);
            if (!directory && !identicalFileBytes(source,dest))
                return fail(op,ExecutionFailureReason::EXECUTION_ERROR,"Copied file failed byte-for-byte source verification: "+Utf8::path(dest));
            auto copied=filesystemSuccess(op,"Copied to: "+Utf8::path(dest)+(directory?" | directory_exists=true":" | source_bytes_equal=true"),dest);
            copied.postconditionVerified=true;
            copied.verifiedKind=directory ? "directory_exists" : "file_content";
            return copied;
        } catch (const fs::filesystem_error& e) { return fail(op, ExecutionFailureReason::EXECUTION_ERROR, e.what()); }
    }

    if (op.type == PrimitiveOperationType::FILESYSTEM_RENAME && op.name == "filesystem.rename") {
        fs::path target; ExecutionResult pathFailure;
        if (!resolvePathForOperation(op, op.parameters.target, FilesystemAccess::RENAME, target, pathFailure) || !leafName(op.parameters.newName))
            return fail(op, ExecutionFailureReason::UNSUPPORTED_ACTION, "Rename requires an authorized target and a single-component newName.", false);
        try {
            if (!fs::exists(target)) return fail(op, ExecutionFailureReason::TARGET_NOT_FOUND, "Rename target does not exist: " + Utf8::path(target));
            fs::path dest = target.parent_path() / Utf8::toPath(op.parameters.newName);
            if (fs::exists(dest)) return fail(op, ExecutionFailureReason::EXECUTION_ERROR, "Rename destination already exists: " + Utf8::path(dest));
            fs::rename(target, dest);
            if (fs::exists(target) || !fs::exists(dest))
                return fail(op,ExecutionFailureReason::EXECUTION_ERROR,"Rename postcondition could not be verified.");
            auto renamed=filesystemSuccess(op,"Renamed to: "+Utf8::path(dest)+" | source_absent=true",dest);
            renamed.postconditionVerified=true;
            renamed.verifiedKind=fs::is_directory(dest)?"directory_exists":"file_content";
            return renamed;
        } catch (const fs::filesystem_error& e) { return fail(op, ExecutionFailureReason::EXECUTION_ERROR, e.what()); }
    }

    if (op.type == PrimitiveOperationType::FILESYSTEM_MOVE && op.name == "filesystem.move") {
        fs::path source, dest; ExecutionResult pathFailure;
        if (!resolvePathForOperation(op, op.parameters.source, FilesystemAccess::MOVE, source, pathFailure) ||
            !resolvePathForOperation(op, op.parameters.destination, FilesystemAccess::MOVE, dest, pathFailure, source.parent_path())) return pathFailure;
        try {
            if (!fs::exists(source)) return fail(op, ExecutionFailureReason::TARGET_NOT_FOUND, "Move source does not exist: " + Utf8::path(source));
            if (fs::exists(dest)) return fail(op, ExecutionFailureReason::EXECUTION_ERROR, "Move destination already exists.");
            if (!fs::is_directory(dest.parent_path())) return fail(op, ExecutionFailureReason::TARGET_NOT_FOUND,
                "Move destination parent does not exist: " + Utf8::path(dest.parent_path()) + " | resolved_destination=" + Utf8::path(dest));
            if (fs::is_directory(source)) {
                fs::path rel = dest.lexically_relative(source);
                if (!rel.empty() && *rel.begin() != "..") return fail(op, ExecutionFailureReason::UNSUPPORTED_ACTION, "Cannot move a directory inside itself.");
            }
            fs::rename(source, dest);
            if (fs::exists(source) || !fs::exists(dest))
                return fail(op,ExecutionFailureReason::EXECUTION_ERROR,"Move postcondition could not be verified.");
            auto moved=filesystemSuccess(op,"Moved to: "+Utf8::path(dest)+" | source_absent=true",dest);
            moved.postconditionVerified=true;
            moved.verifiedKind=fs::is_directory(dest)?"directory_exists":"file_content";
            return moved;
        } catch (const fs::filesystem_error& e) { return fail(op, ExecutionFailureReason::EXECUTION_ERROR, string(e.what()) + " (rename-based move can fail across volumes)"); }
    }

    if (op.type == PrimitiveOperationType::FILESYSTEM_DELETE && op.name == "filesystem.delete") {
        fs::path target; ExecutionResult pathFailure;
        if (!resolvePathForOperation(op, op.parameters.target, FilesystemAccess::DELETE_RESOURCE, target, pathFailure)) return pathFailure;
        bool recursive = false;
        if (!boolParameter(op.parameters.recursive, false, recursive))
            return fail(op, ExecutionFailureReason::INVALID_PLAN, "recursive must be true/false or 1/0.", false);
        try {
            if (!fs::exists(target)) return fail(op, ExecutionFailureReason::TARGET_NOT_FOUND, "Delete target does not exist: " + Utf8::path(target));
            if (fs::is_directory(target)) {
                if (!recursive && !fs::is_empty(target)) return fail(op, ExecutionFailureReason::EXECUTION_ERROR, "Directory is not empty; recursive deletion was not enabled.");
                if (recursive) fs::remove_all(target); else fs::remove(target);
            } else fs::remove(target);
            return filesystemSuccess(op, "Deleted: " + Utf8::path(target), target);
        } catch (const fs::filesystem_error& e) { return fail(op, ExecutionFailureReason::EXECUTION_ERROR, e.what()); }
    }

    if (op.type == PrimitiveOperationType::FILESYSTEM_LIST_DIRECTORY && op.name == "filesystem.list_directory") {
        fs::path directory; ExecutionResult pathFailure;
        if (!resolvePathForOperation(op, op.parameters.location, FilesystemAccess::DISCOVER, directory, pathFailure)) return pathFailure;
        int limit = 100;
        try { if (!op.parameters.maxResults.empty()) limit = stoi(op.parameters.maxResults); }
        catch (...) { return fail(op, ExecutionFailureReason::INVALID_PLAN, "max_results must be an integer.", false); }
        if (limit < 1 || limit > 200) return fail(op, ExecutionFailureReason::INVALID_PLAN, "max_results must be from 1 to 200.", false);
        try {
            if (!fs::is_directory(directory)) return fail(op, ExecutionFailureReason::TARGET_NOT_FOUND, "Directory does not exist: " + Utf8::path(directory));
            vector<string> entries;
            for (const auto& entry : fs::directory_iterator(directory, fs::directory_options::skip_permission_denied)) {
                entries.push_back(Utf8::path(entry.path()));
                if (static_cast<int>(entries.size()) == limit) break;
            }
            return filesystemSuccess(op, "Listed " + to_string(entries.size()) + " entries in: " + Utf8::path(directory), directory, entries);
        } catch (const fs::filesystem_error& e) { return fail(op, ExecutionFailureReason::EXECUTION_ERROR, e.what()); }
    }

    if (op.type == PrimitiveOperationType::FILESYSTEM_SEARCH && op.name == "filesystem.search") {
        fs::path root; ExecutionResult pathFailure;
        if (!resolvePathForOperation(op, op.parameters.location, FilesystemAccess::DISCOVER, root, pathFailure)) return pathFailure;
        int limit = 100;
        try { if (!op.parameters.maxResults.empty()) limit = stoi(op.parameters.maxResults); }
        catch (...) { return fail(op, ExecutionFailureReason::INVALID_PLAN, "max_results must be an integer.", false); }
        if (op.parameters.query.empty() || limit < 1 || limit > 1000)
            return fail(op, ExecutionFailureReason::INVALID_PLAN, "search requires query and max_results from 1 to 1000.", false);
        const string type = lower(op.parameters.entryType.empty() ? "any" : op.parameters.entryType);
        if (type != "any" && type != "file" && type != "directory")
            return fail(op, ExecutionFailureReason::INVALID_PLAN, "entry_type must be any, file, or directory.", false);
        try {
            if (!fs::is_directory(root)) return fail(op, ExecutionFailureReason::TARGET_NOT_FOUND, "Search root does not exist: " + Utf8::path(root));
            double days=-1;
            try {if(!op.parameters.modifiedWithinDays.empty())days=std::stod(op.parameters.modifiedWithinDays);}catch(...){return fail(op,ExecutionFailureReason::INVALID_PLAN,"Invalid recency bound.",false);}
            if(!std::isfinite(days) || days>36500 || days< -1)return fail(op,ExecutionFailureReason::INVALID_PLAN,"Invalid recency bound.",false);
            vector<nlohmann::json> ranked;
            error_code iteratorError;
            fs::recursive_directory_iterator iterator(root,fs::directory_options::none,iteratorError),end;
            bool incomplete=bool(iteratorError);size_t scanned=0;
            // Bound traversal independently of result count, then sort before truncating.
            constexpr size_t scanLimit=50000;
            while (!iteratorError && iterator!=end && scanned<scanLimit) {
                ++scanned;const auto entry=*iterator;error_code error;
                const bool file=entry.is_regular_file(error);const bool directory=entry.is_directory(error);
                if(!error && (type=="any" || type=="file" && file || type=="directory" && directory)) {
                    auto row=FilesystemRanking::rank(op.parameters.query,Utf8::path(entry.path().filename()));
                    if(op.parameters.query=="*" && (!op.parameters.extension.empty() || days>=0))row={{"match_score",0.85},{"match_reasons",nlohmann::json::array()},{"exact_name_match",false}};
                    if(!op.parameters.extension.empty() && lower(Utf8::path(entry.path().extension()))!=lower(op.parameters.extension))row["match_score"]=0;
                    const auto timestamp=entry.last_write_time(error);
                    if(days>=0 && (error || std::chrono::duration<double,std::ratio<86400>>(fs::file_time_type::clock::now()-timestamp).count()>days))row["match_score"]=0;
                    if(row["match_score"].get<double>()>=FilesystemRanking::minimumScore) {
                        const auto canonical=fs::canonical(entry.path(),error);
                        if(!error) {
                            row["path"]=Utf8::path(canonical);row["name"]=Utf8::path(canonical.filename());
                            row["parent_path"]=Utf8::path(canonical.parent_path());row["entry_type"]=directory?"directory":file?"file":"other";
                            row["extension"]=Utf8::path(canonical.extension());
                            const auto modified=entry.last_write_time(error);
                            if(!error) {
                                row["modified_time_ticks"]=modified.time_since_epoch().count();
                                row["modified_age_days"]=std::chrono::duration<double,std::ratio<86400>>(fs::file_time_type::clock::now()-modified).count();
                                const auto systemTime=std::chrono::file_clock::to_sys(modified);
                                const auto time=std::chrono::system_clock::to_time_t(systemTime);std::tm utc{};
                                if(gmtime_s(&utc,&time)==0){std::ostringstream formatted;formatted<<std::put_time(&utc,"%Y-%m-%dT%H:%M:%SZ");row["modified_time"]=formatted.str();}
                            }
                            row["match_reasons"].push_back("location_match");
                            if(!op.parameters.extension.empty())row["match_reasons"].push_back("extension_match");
                            if(days>=0)row["match_reasons"].push_back("recency_match");
                            if(type!="any")row["match_reasons"].push_back("entry_type_match");
                            ranked.push_back(std::move(row));
                        }else incomplete=true;
                    }
                }
                iterator.increment(iteratorError);if(iteratorError){incomplete=true;iteratorError.clear();}
            }
            if(scanned==scanLimit && iterator!=end)incomplete=true;
            std::sort(ranked.begin(),ranked.end(),[](const auto& a,const auto& b){
                if(a["match_score"]!=b["match_score"])return a["match_score"].template get<double>()>b["match_score"].template get<double>();
                return a["path"].template get<string>()<b["path"].template get<string>();
            });
            const auto total=ranked.size();if(ranked.size()>static_cast<size_t>(limit))ranked.resize(limit);
            vector<string> matches;for(size_t i=0;i<ranked.size();++i){ranked[i]["candidate_id"]="candidate_"+to_string(i+1);matches.push_back(ranked[i]["path"].get<string>());}
            auto result=filesystemSuccess(op,"Found "+to_string(matches.size())+" ranked entries under: "+Utf8::path(root),root,matches);
            auto facts=nlohmann::json::parse(result.factsJson);
            facts.update({{"category","FILESYSTEM_SEARCH_RESULT"},{"search_root",Utf8::path(root)},{"query",op.parameters.query},
                {"results",ranked},{"search_complete",!incomplete},{"results_truncated",total>ranked.size()},
                {"scanned_entries",scanned},{"clear_winner",!incomplete && (total<2 || ranked.size()>1) && FilesystemRanking::clear(ranked)}});
            result.factsJson=Utf8::dump(facts,"filesystem.search.facts");return result;
        } catch (const fs::filesystem_error& e) { return fail(op, ExecutionFailureReason::EXECUTION_ERROR, e.what()); }
    }

    if (op.type == PrimitiveOperationType::FILESYSTEM_INSPECT && op.name == "filesystem.inspect") {
        fs::path target; ExecutionResult pathFailure;
        if (!resolvePathForOperation(op, op.parameters.target, FilesystemAccess::DISCOVER, target, pathFailure)) return pathFailure;
        try {
            if (!fs::exists(target)) return filesystemSuccess(op, "Inspected path: " + Utf8::path(target) + " | exists=false", target);
            const string kind = fs::is_directory(target) ? "directory" : (fs::is_regular_file(target) ? "file" : "other");
            string detail="Inspected " + kind + ": " + Utf8::path(target) + " | exists=true";
            nlohmann::json contentFacts=nlohmann::json::object();
            if (fs::is_regular_file(target)) {
                const auto bytes=fs::file_size(target);
                detail += " | bytes=" + to_string(bytes);
                if (bytes<=65536) {
                    ifstream input(target, ios::binary);
                    const string content((istreambuf_iterator<char>(input)), istreambuf_iterator<char>());
                    if (input && Utf8::invalidOffset(content)==string::npos) {
                        contentFacts["text"]=Utf8::truncate(content,8192,"file.preview");
                        contentFacts["preview_truncated"]=bytes>8192;
                        detail += " | text=" + Utf8::dump(nlohmann::json(Utf8::truncate(content,8192,"file.preview")),"file.preview") + (bytes>8192 ? " | preview_truncated=true" : " | preview_truncated=false");
                    }
                }
            }
            auto inspected=filesystemSuccess(op, detail, target, {Utf8::path(target)});
            auto facts=nlohmann::json::parse(inspected.factsJson);facts.update(contentFacts);inspected.factsJson=Utf8::dump(facts,"filesystem.inspect.facts");
            return inspected;
        } catch (const fs::filesystem_error& e) { return fail(op, ExecutionFailureReason::EXECUTION_ERROR, e.what()); }
    }

    
    if (op.name == "browser.observe" || op.name == "browser.click" || op.name == "browser.type" || op.name=="browser.download" || op.name=="browser.tabs" || op.name=="browser.select_tab") {
        nlohmann::json request;
        if (op.name == "browser.observe") {
            request = {{"command", "observe"}};
        } else if (op.name=="browser.tabs" || op.name=="browser.select_tab") {
            request={{"command",op.name=="browser.tabs" ? "tabs" : "select_tab"},{"target",op.parameters.target}};
        } else {
            const bool typing = op.name == "browser.type";
            if (op.parameters.elementId.empty() &&
                (op.parameters.role.empty() || op.parameters.text.empty())) {
                return fail(op, ExecutionFailureReason::INVALID_PLAN,
                    "Browser action requires element_id or both role and text.", false);
            }
            request = {{"command", typing ? "type" : op.name=="browser.download" ? "download" : "click"},
                       {"element_id", op.parameters.elementId},
                       {"role", op.parameters.role},
                       {"text", op.parameters.text}};
            if (typing) request["content"] = op.parameters.content;
            if (op.name=="browser.download") {
                fs::path directory; ExecutionResult pathFailure;
                if (!resolvePathForOperation(op,op.parameters.location.empty() ? "downloads" : op.parameters.location,FilesystemAccess::CREATE,directory,pathFailure)) return pathFailure;
                if (!fs::is_directory(directory)) return fail(op,ExecutionFailureReason::TARGET_NOT_FOUND,"Download directory does not exist.");
                request["location"]=Utf8::path(directory);
                if (!op.parameters.timeoutMs.empty()) request["timeout_ms"]=stoi(op.parameters.timeoutMs);
            }
        }
        nlohmann::json response;
        string error;
        if (!browserAgentRequest(request, response, error)) {
            auto failed = fail(op, ExecutionFailureReason::EXECUTION_ERROR, "Browser control failed: " + error);
            failed.failureCategory = response.value("failure_category", string("browser_unavailable"));
            return failed;
        }
        string message = response.value("message", string("Browser operation completed."));
        // Preserve wire data separately from concise terminal/planner diagnostics.
        // The runtime always obtains a fresh observation before another DOM action.
        auto result = makeResult(op, ExecutionStatus::SUCCESS, ExecutionFailureReason::NONE, message, true, true);
        result.browserData = Utf8::dump(response,"browser.response");
        if (op.name=="browser.download" && response.value("completed",false)) {
            result.resolvedPath=response.value("path",string{});
            if (result.resolvedPath.empty() || !fs::is_regular_file(Utf8::toPath(result.resolvedPath)))
                return fail(op,ExecutionFailureReason::EXECUTION_ERROR,"Browser download reported completion but no regular file was verified.");
            result.postconditionVerified=true;
            result.verifiedKind="file_content";
            result.message += " | resolved_path="+result.resolvedPath+" | bytes="+to_string(response.value("bytes",0LL))+" | completed=true";
        }
        return result;
    }

    if ((op.type == PrimitiveOperationType::BROWSER_NAVIGATE && op.name == "browser.navigate") ||
        (op.type == PrimitiveOperationType::BROWSER_WAIT_FOR && op.name == "browser.wait_for")) {
        const bool navigating = op.type == PrimitiveOperationType::BROWSER_NAVIGATE;
        string validationError;
        if (navigating && !validHttpUrl(op.parameters.url, validationError))
            return fail(op, ExecutionFailureReason::INVALID_PLAN, validationError, false);

        const string condition = op.parameters.readyCondition.empty()
            ? string("domcontentloaded") : op.parameters.readyCondition;
        const set<string> allowedConditions = {"domcontentloaded", "load", "networkidle", "selector"};
        if (!allowedConditions.contains(condition))
            return fail(op, ExecutionFailureReason::INVALID_PLAN,
                "ready_condition must be domcontentloaded, load, networkidle, or selector.", false);
        if (condition == "selector" && op.parameters.selector.empty())
            return fail(op, ExecutionFailureReason::INVALID_PLAN,
                "selector is required when ready_condition is selector.", false);
        if (!op.parameters.timeoutMs.empty()) {
            try {
                size_t used = 0;
                const int timeout = stoi(op.parameters.timeoutMs, &used);
                if (used != op.parameters.timeoutMs.size() || timeout < 1000 || timeout > 120000)
                    throw invalid_argument("timeout");
            } catch (...) {
                return fail(op, ExecutionFailureReason::INVALID_PLAN,
                    "timeout_ms must be an integer from 1000 to 120000.", false);
            }
        }
        nlohmann::json request = {
            {"command", navigating ? "navigate" : "wait_for"},
            {"ready_condition", condition},
            {"timeout_ms", op.parameters.timeoutMs.empty() ? 30000 : stoi(op.parameters.timeoutMs)},
            {"selector", op.parameters.selector}
        };
        if (navigating) request["url"] = op.parameters.url;
        nlohmann::json response;
        string error;
        if (!browserAgentRequest(request, response, error)) {
            auto failed = fail(op, ExecutionFailureReason::EXECUTION_ERROR, "Browser control failed: " + error);
            failed.failureCategory = response.value("failure_category", string("browser_unavailable"));
            return failed;
        }
        const string currentUrl = response.value("url", string("unknown URL"));
        const string title = response.value("title", string{});
        string domSummary;
        if (response.contains("dom") && response["dom"].is_object()) {
            const auto& dom = response["dom"];
            domSummary = " DOM readyState=" + dom.value("readyState", string("unknown")) +
                ", documentElement=" + string(dom.value("hasDocumentElement", false) ? "present" : "missing") +
                ", body=" + string(dom.value("hasBody", false) ? "present" : "missing") +
                ", bodyVisible=" + string(dom.value("bodyVisible", false) ? "true" : "false") + ".";
        }
        const string selectorSummary = condition == "selector"
            ? " Visible selector confirmed: " + op.parameters.selector + "." : string{};
        const string message = (navigating ? "Navigated to " : "DOM readiness confirmed at ") +
            currentUrl + (title.empty() ? string{} : " (" + title + ")") +
            ". Readiness condition '" + condition + "' satisfied." + domSummary + selectorSummary;
        return makeResult(op, ExecutionStatus::SUCCESS, ExecutionFailureReason::NONE,
            message, true, true);
    }

    if(op.type==PrimitiveOperationType::APPLICATION_OBSERVE && op.name=="application.observe") {
        if(op.parameters.processId.empty() && op.parameters.application.empty())return fail(op,ExecutionFailureReason::INVALID_PLAN,"application.observe requires an observed process_id or exact executable application name.",false);
        std::optional<DWORD> pid;std::optional<uintptr_t> window;
        try {
            if(!op.parameters.processId.empty()){size_t used=0;auto value=std::stoull(op.parameters.processId,&used);if(used!=op.parameters.processId.size() || value==0 || value>MAXDWORD)throw std::invalid_argument("pid");pid=static_cast<DWORD>(value);}
            if(!op.parameters.windowHandle.empty()){size_t used=0;auto value=std::stoull(op.parameters.windowHandle,&used);if(used!=op.parameters.windowHandle.size() || value==0 || value>UINTPTR_MAX)throw std::invalid_argument("window");window=static_cast<uintptr_t>(value);}
        }catch(...){return fail(op,ExecutionFailureReason::INVALID_PLAN,"Invalid process_id or decimal window_handle.",false);}
        fs::path target;
        if(!op.parameters.target.empty()){ExecutionResult pathFailure;if(!resolvePathForOperation(op,op.parameters.target,FilesystemAccess::DISCOVER,target,pathFailure))return pathFailure;}
        const auto executable=op.parameters.application.empty()?string{}:appExeFromName(op.parameters.application);
        if(!op.parameters.application.empty() && executable.empty())return fail(op,ExecutionFailureReason::TARGET_NOT_FOUND,"No uniquely observed executable matches application.observe scope.",false);
        auto observed=makeResult(op,ExecutionStatus::SUCCESS,ExecutionFailureReason::NONE,"Scoped application/window observation completed; only observed associations are evidence.",true,true);
        observed.factsJson=Utf8::dump(ApplicationObservation::observe(pid,executable,target,window),"application.observe");return observed;
    }
    if (op.type == PrimitiveOperationType::APPLICATION_LAUNCH &&
        op.name == "application.launch") {

        if (op.parameters.application.empty() && op.parameters.target.empty()) {
            return fail(
                op,
                ExecutionFailureReason::UNSUPPORTED_ACTION,
                "Provide a registered application or an existing target path.",
                false
            );
        }

        fs::path target;

        if (!op.parameters.target.empty()) {
            if (!launchTargetIsSafe(op.parameters.target, target)) {
                return fail(
                    op,
                    ExecutionFailureReason::UNSUPPORTED_ACTION,
                    "Launch target must be an existing authorized filesystem path.",
                    false
                );
            }
        }

        if (op.parameters.application.empty()) {
            const auto started=ApplicationObservation::now();
            SHELLEXECUTEINFOW launch{};launch.cbSize=sizeof(launch);launch.fMask=SEE_MASK_NOCLOSEPROCESS|SEE_MASK_FLAG_NO_UI;launch.lpVerb=L"open";launch.lpFile=target.c_str();launch.nShow=SW_SHOWNORMAL;
            if (!ShellExecuteExW(&launch))
                return fail(op,ExecutionFailureReason::EXECUTION_ERROR,"Windows could not open the requested target using its registered association.");
            auto dispatched=makeResult(op,ExecutionStatus::SUCCESS,ExecutionFailureReason::NONE,"Windows accepted opening the verified target: "+Utf8::path(target),true,true);
            auto facts=nlohmann::json::parse(dispatched.factsJson);facts["target_requested"]=op.parameters.target;facts["target_path"]=Utf8::path(target);facts["mechanism"]="shell_association";
            ApplicationObservation::recordLaunch(facts,launch.hProcess,started,Utf8::toPath(op.parameters.target));
            if(launch.hProcess)CloseHandle(launch.hProcess);dispatched.factsJson=Utf8::dump(facts,"application.dispatch");return dispatched;
        }
        ApplicationResolver resolver;
        const ApplicationResolverResult resolved =
            resolver.resolve(op.parameters.application);

        if (!resolved.success || resolved.appId.empty()) {
            return fail(
                op,
                ExecutionFailureReason::TARGET_NOT_FOUND,
                resolved.message.empty()
                    ? "Could not resolve the registered Windows application."
                    : resolved.message,
                false
            );
        }

        const auto app=Utf8::toPath(resolved.appId);
        if (!target.empty() && resolved.appId.find('!')!=string::npos) {
            const auto started=ApplicationObservation::now();
            const auto initialized=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
            IApplicationActivationManager* manager=nullptr;
            IShellItem* item=nullptr; IShellItemArray* items=nullptr; DWORD pid=0;
            HRESULT status=CoCreateInstance(CLSID_ApplicationActivationManager,nullptr,CLSCTX_LOCAL_SERVER,IID_PPV_ARGS(&manager));
            if (SUCCEEDED(status) && resolved.acceptsCommandLine) {
                const auto arguments=L"\""+target.native()+L"\"";
                status=manager->ActivateApplication(app.c_str(),arguments.c_str(),AO_NONE,&pid);
            } else {
                if (SUCCEEDED(status)) status=SHCreateItemFromParsingName(target.c_str(),nullptr,IID_PPV_ARGS(&item));
                if (SUCCEEDED(status)) status=SHCreateShellItemArrayFromShellItem(item,IID_PPV_ARGS(&items));
                if (SUCCEEDED(status)) status=manager->ActivateForFile(app.c_str(),items,L"open",&pid);
            }
            if (items) items->Release(); if (item) item->Release(); if (manager) manager->Release();
            if (SUCCEEDED(initialized)) CoUninitialize();
            if (FAILED(status)) return fail(op,ExecutionFailureReason::EXECUTION_ERROR,"Registered application could not open the requested file (HRESULT "+to_string(static_cast<unsigned long>(status))+").");
            auto activated=makeResult(op,ExecutionStatus::SUCCESS,ExecutionFailureReason::NONE,"Application activation accepted with requested file (PID "+to_string(pid)+"): "+Utf8::path(target),true,true);
            auto facts=nlohmann::json::parse(activated.factsJson);if(pid)facts["process_id"]=pid;facts["target_requested"]=op.parameters.target;facts["target_path"]=Utf8::path(target);facts["mechanism"]=resolved.acceptsCommandLine?"activate_application":"activate_for_file";
            HANDLE process=pid?OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid):nullptr;
            ApplicationObservation::recordLaunch(facts,process,started,Utf8::toPath(op.parameters.target));if(process)CloseHandle(process);
            activated.factsJson=Utf8::dump(facts,"application.activation.facts");return activated;
        }
        const bool desktopExecutable=lower(Utf8::path(app.extension()))==".exe";
        if (!target.empty() && !desktopExecutable)
            return fail(op,ExecutionFailureReason::UNSUPPORTED_ACTION,"This application registration does not expose file activation. The requested file was not ignored.",false);
        const auto shellTarget=desktopExecutable ? app : Utf8::toPath("shell:AppsFolder\\"+resolved.appId);
        const auto arguments=target.empty() ? std::wstring{} : L"\""+target.native()+L"\"";
        SHELLEXECUTEINFOW launch{};launch.cbSize=sizeof(launch);launch.fMask=SEE_MASK_NOCLOSEPROCESS|SEE_MASK_FLAG_NO_UI;launch.lpVerb=L"open";launch.lpFile=shellTarget.c_str();launch.lpParameters=arguments.empty()?nullptr:arguments.c_str();launch.nShow=SW_SHOWNORMAL;

        const auto started=ApplicationObservation::now();
        if (!ShellExecuteExW(&launch)) {
            return fail(
                op,
                ExecutionFailureReason::EXECUTION_ERROR,
                "Windows failed to launch application: " +
                    resolved.displayName,
                false
            );
        }

        auto dispatched=makeResult(
            op,
            ExecutionStatus::SUCCESS,
            ExecutionFailureReason::NONE,
            "Launch request accepted for " + resolved.displayName + ".",
            true,
            true
        );
        auto facts=nlohmann::json::parse(dispatched.factsJson);facts["target_requested"]=op.parameters.target;if(!target.empty())facts["target_path"]=Utf8::path(target);facts["mechanism"]="shell_execute";
        ApplicationObservation::recordLaunch(facts,launch.hProcess,started,Utf8::toPath(op.parameters.target));
        if(launch.hProcess)CloseHandle(launch.hProcess);dispatched.factsJson=Utf8::dump(facts,"application.dispatch");return dispatched;
    }


    if (op.type == PrimitiveOperationType::APPLICATION_FOCUS && op.name == "application.focus") {
        const string exe = appExeFromName(op.parameters.application);
        if (exe.empty())
            return fail(op, ExecutionFailureReason::UNSUPPORTED_ACTION,
                "No unique visible application matches. Use an executable name observed by environment.observe: " + op.parameters.application, false);

        vector<DWORD> pids;
        // Snapshot processes, then search for matching image names.
        DWORD processIds[4096]{};
        DWORD bytes = 0;
        if (!EnumProcesses(processIds, sizeof(processIds), &bytes))
            return fail(op, ExecutionFailureReason::EXECUTION_ERROR, "EnumProcesses failed.");

        const size_t count = bytes / sizeof(DWORD);
        for (size_t i = 0; i < count; ++i)
            if (sameProcessImage(processIds[i], exe)) pids.push_back(processIds[i]);

        const string title = op.parameters.target;
        for (DWORD pid : pids) {
            HWND hwnd = findWindowForPid(pid, title);
            if (hwnd) {
                ShowWindow(hwnd, SW_RESTORE);
                SetForegroundWindow(hwnd);
                if (GetForegroundWindow() == hwnd) {
                    auto focused=makeResult(op, ExecutionStatus::SUCCESS, ExecutionFailureReason::NONE,
                        "Focused " + op.parameters.application + " window (PID " + to_string(pid) + ").", true, true);
                    auto facts=nlohmann::json::parse(focused.factsJson);facts["process_id"]=pid;facts["window_observed"]=true;facts["foreground_verified"]=true;focused.factsJson=Utf8::dump(facts,"application.focus.facts");return focused;
                }
                return fail(op, ExecutionFailureReason::EXECUTION_ERROR,
                    "Windows did not grant foreground focus. The target window was found.");
            }
        }
        return fail(op, ExecutionFailureReason::TARGET_NOT_FOUND,
            "No visible window found for " + op.parameters.application +
            (title.empty() ? string{} : " matching title: " + title));
    }

    if (op.type == PrimitiveOperationType::APPLICATION_CLOSE && op.name == "application.close") {
        const string exe = appExeFromName(op.parameters.application);
        if (exe.empty())
            return fail(op, ExecutionFailureReason::UNSUPPORTED_ACTION,
                "No unique visible application matches. Use an executable name observed by environment.observe: " + op.parameters.application, false);

        DWORD processIds[4096]{};
        DWORD bytes = 0;
        if (!EnumProcesses(processIds, sizeof(processIds), &bytes))
            return fail(op, ExecutionFailureReason::EXECUTION_ERROR, "EnumProcesses failed.");

        const string title = op.parameters.target;
        const size_t count = bytes / sizeof(DWORD);
        for (size_t i = 0; i < count; ++i) {
            const DWORD pid = processIds[i];
            if (!sameProcessImage(pid, exe)) continue;
            HWND hwnd = findWindowForPid(pid, title);
            if (!hwnd) continue;

            // Close the selected window politely; do not terminate the process.
            if (PostMessageA(hwnd, WM_CLOSE, 0, 0)) {
                auto closed=makeResult(op, ExecutionStatus::SUCCESS, ExecutionFailureReason::NONE,
                    "Close request sent to " + op.parameters.application +
                    " window (PID " + to_string(pid) + ").", true, true);
                auto facts=nlohmann::json::parse(closed.factsJson);facts["process_id"]=pid;facts["window_observed"]=true;facts["close_requested"]=true;closed.factsJson=Utf8::dump(facts,"application.close.facts");return closed;
            }
            return fail(op, ExecutionFailureReason::EXECUTION_ERROR, "Failed to send WM_CLOSE to target window.");
        }
        return fail(op, ExecutionFailureReason::TARGET_NOT_FOUND,
            "No visible window found to close for " + op.parameters.application +
            (title.empty() ? string{} : " matching title: " + title));
    }


    if (op.type == PrimitiveOperationType::TEXT_WRITE && op.name == "text.write") {
        // LLM-generated file targets sometimes include a trailing separator,
        // which turns a regular filename into a directory-like path on Windows.
        string targetText = op.parameters.target;
        while (targetText.size() > 1 &&
               (targetText.back() == '\\' || targetText.back() == '/') &&
               !(targetText.size() == 3 && targetText[1] == ':')) {
            targetText.pop_back();
        }
        fs::path target; ExecutionResult pathFailure;
        if (!resolvePathForOperation(op, targetText, FilesystemAccess::WRITE, target, pathFailure)) return pathFailure;
        // The resolver may append a directory separator to an existing file.
        // Normalize that spelling only after the original target was authorized.
        if (target.has_relative_path() && target.filename().empty()) target = target.parent_path();
        try {
            if (!fs::is_regular_file(target))
                return fail(op, ExecutionFailureReason::TARGET_NOT_FOUND,
                    "text.write target must be an existing regular file: " + Utf8::path(target));
            ofstream file(target, ios::binary | ios::trunc);
            if (!file.is_open())
                return fail(op, ExecutionFailureReason::EXECUTION_ERROR,
                    "Unable to open target for writing: " + Utf8::path(target));
            file.write(op.parameters.content.data(),
                static_cast<streamsize>(op.parameters.content.size()));
            file.close();
            if (!file)
                return fail(op, ExecutionFailureReason::EXECUTION_ERROR,
                    "Failed while writing target: " + Utf8::path(target));
            return filesystemSuccess(op, "Text written to file: " + Utf8::path(target), target);
        } catch (const fs::filesystem_error& e) {
            return fail(op, ExecutionFailureReason::EXECUTION_ERROR, e.what());
        }
    }

    if (op.type == PrimitiveOperationType::KEYBOARD_TYPE && op.name == "keyboard.type") {
        HWND targetWindow = nullptr;
        if (!focusWindowByTitle(op.parameters.target, targetWindow))
            return fail(op, ExecutionFailureReason::TARGET_NOT_FOUND,
                "No visible window matched keyboard.type target: " + op.parameters.target);
        if (targetWindow) {
            SetForegroundWindow(targetWindow);
            SetFocus(targetWindow);
            Sleep(80);
        }
        string error;
        if (!sendUtf8Text(op.parameters.content, error))
            return fail(op, ExecutionFailureReason::EXECUTION_ERROR, error);
        return makeResult(op, ExecutionStatus::SUCCESS, ExecutionFailureReason::NONE,
            "Typed " + to_string(op.parameters.content.size()) + " UTF-8 bytes.",
            true, true);
    }

    if (op.type == PrimitiveOperationType::KEYBOARD_PRESS && op.name == "keyboard.press") {
        const WORD key = virtualKeyFor(op.parameters.key);
        if (!key)
            return fail(op, ExecutionFailureReason::UNSUPPORTED_ACTION,
                "Unsupported key name: " + op.parameters.key, false);

        vector<WORD> modifiers;
        if (!parseModifiers(op.parameters.modifiers, modifiers))
            return fail(op, ExecutionFailureReason::INVALID_PLAN,
                "modifiers must be a comma-separated list of CTRL, ALT, SHIFT, or WIN.", false);

        int repeatCount = 1;
        if (!op.parameters.repeatCount.empty()) {
            try {
                size_t used = 0;
                repeatCount = stoi(op.parameters.repeatCount, &used);
                if (used != op.parameters.repeatCount.size() || repeatCount < 1 || repeatCount > 100)
                    throw invalid_argument("range");
            } catch (...) {
                return fail(op, ExecutionFailureReason::INVALID_PLAN,
                    "repeatCount must be an integer from 1 to 100.", false);
            }
        }

        string error;
        if (!sendKeyCombo(key, modifiers, repeatCount, error))
            return fail(op, ExecutionFailureReason::EXECUTION_ERROR, error);
        return makeResult(op, ExecutionStatus::SUCCESS, ExecutionFailureReason::NONE,
            "Key event sent: " + op.parameters.key + " (repeatCount=" +
            to_string(repeatCount) + ").", true, true);
    }


    if (op.type == PrimitiveOperationType::MOUSE_MOVE && op.name == "mouse.move") {
        int x = 0, y = 0;
        if (!parseCoordinate(op.parameters.x, x) || !parseCoordinate(op.parameters.y, y))
            return fail(op, ExecutionFailureReason::INVALID_PLAN,
                "mouse.move requires integer x and y screen coordinates.", false);
        string error;
        if (!moveCursorTo(x, y, error))
            return fail(op, ExecutionFailureReason::EXECUTION_ERROR, error);
        return makeResult(op, ExecutionStatus::SUCCESS, ExecutionFailureReason::NONE,
            "Cursor moved to screen coordinates (" + to_string(x) + ", " +
            to_string(y) + ") and position verified.", true, true);
    }

    if (op.type == PrimitiveOperationType::MOUSE_CLICK && op.name == "mouse.click") {
        int x = 0, y = 0, clickCount = 1;
        if (!parseCoordinate(op.parameters.x, x) || !parseCoordinate(op.parameters.y, y))
            return fail(op, ExecutionFailureReason::INVALID_PLAN,
                "mouse.click requires integer x and y screen coordinates.", false);
        if (!parseClickCount(op.parameters.clickCount, clickCount))
            return fail(op, ExecutionFailureReason::INVALID_PLAN,
                "clickCount must be 1 or 2.", false);
        const string button = lower(op.parameters.button.empty() ? "left" : op.parameters.button);
        if (button != "left" && button != "l" && button != "right" &&
            button != "r" && button != "middle" && button != "m")
            return fail(op, ExecutionFailureReason::UNSUPPORTED_ACTION,
                "button must be left, right, or middle.", false);

        string error;
        if (!moveCursorTo(x, y, error))
            return fail(op, ExecutionFailureReason::EXECUTION_ERROR, error);
        Sleep(40);
        if (!sendMouseClick(button, clickCount, error))
            return fail(op, ExecutionFailureReason::EXECUTION_ERROR, error);
        return makeResult(op, ExecutionStatus::SUCCESS, ExecutionFailureReason::NONE,
            "Sent " + to_string(clickCount) + " " + button +
            " click(s) at screen coordinates (" + to_string(x) + ", " +
            to_string(y) + ").", true, true);
    }

    return makeResult(op, ExecutionStatus::NOT_EXECUTED, ExecutionFailureReason::UNSUPPORTED_ACTION,
        "No execution mechanism exists for primitive: " + op.name);
}
