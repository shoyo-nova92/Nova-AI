#include <windows.h>
#include <winhttp.h>
#include <nlohmann/json.hpp>
#include <psapi.h>
#include <shlobj.h>
#include "primitive_execution_engine.hpp"
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
                             ExecutionResult& failure) {
    FilesystemPathPolicy policy;
    const FilesystemPathResult path = policy.resolve(raw);
    if (!path.ok) {
        if (path.error == FilesystemPathError::UNKNOWN_LOCATION) {
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
                return false;
            }
            const FilesystemPathError discoveryError =
                discovery.status == FilesystemLocationResolution::AMBIGUOUS
                    ? FilesystemPathError::AMBIGUOUS_LOCATION : path.error;
            failure = fail(op, failureReasonForPath(discoveryError),
                "Filesystem keyword location search failed (" +
                FilesystemPathPolicy::errorName(discoveryError) + "): " + discovery.message,
                false);
            failure.failureCategory = FilesystemPathPolicy::errorName(discoveryError);
            for (const auto& candidate : discovery.candidates)
                failure.discoveredPaths.push_back(candidate.path.string());
            return false;
        }
        failure = fail(op, failureReasonForPath(path.error),
            "Filesystem path resolution failed (" + FilesystemPathPolicy::errorName(path.error) + "): " + path.message,
            false);
        failure.failureCategory = FilesystemPathPolicy::errorName(path.error);
        return false;
    }
    const FilesystemPathResult authorization = policy.authorize(path.path, access);
    if (!authorization.ok) {
        failure = fail(op, failureReasonForPath(authorization.error),
            "Filesystem path authorization failed (" + FilesystemPathPolicy::errorName(authorization.error) + "): " + authorization.message,
            false);
        failure.resolvedPath = authorization.path.string();
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
    result.resolvedPath = path.string();
    result.discoveredPaths = move(discovered);
    return result;
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
    if (expected.empty()) return false;
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process) return false;
    char buffer[MAX_PATH * 4]{};
    DWORD size = static_cast<DWORD>(sizeof(buffer));
    bool matches = false;
    if (QueryFullProcessImageNameA(process, 0, buffer, &size)) {
        fs::path image(string(buffer, size));
        matches = lower(image.filename().string()) == lower(expected);
    }
    CloseHandle(process);
    return matches;
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

    char title[1024]{};
    GetWindowTextA(hwnd, title, static_cast<int>(sizeof(title)));
    string titleText(title);
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
    const string app = lower(application);
    if (app == "notepad" || app == "notepad.exe") return "notepad.exe";
    if (app == "calculator" || app == "calc" || app == "calc.exe") return "calc.exe";
    if (app == "mspaint" || app == "paint" || app == "mspaint.exe") return "mspaint.exe";
    if (app == "explorer" || app == "file explorer" || app == "explorer.exe") return "explorer.exe";
    if (app == "vscode" || app == "vs code" || app == "code" || app == "code.exe") return "Code.exe";
    if (app == "chrome" || app == "google chrome" || app == "chrome.exe") return "chrome.exe";
    if (app == "edge" || app == "microsoft edge" || app == "msedge.exe") return "msedge.exe";
    return {};
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
        char title[1024]{};
        GetWindowTextA(hwnd, title, static_cast<int>(sizeof(title)));
        string value(title);
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

    char modulePath[MAX_PATH * 4]{};
    const DWORD length = GetModuleFileNameA(
        nullptr, modulePath, static_cast<DWORD>(sizeof(modulePath)));

    if (length == 0 || length >= sizeof(modulePath)) {
        error = "Cannot locate Nova executable to start the browser agent.";
        return false;
    }

    const fs::path root =
        fs::path(string(modulePath, length)).parent_path().parent_path();

    const fs::path script = root / "python" / "browser_agent.py";
    const fs::path venvPython = root / ".venv-win" / "Scripts" / "python.exe";

    if (!fs::exists(script)) {
        error = "Browser agent script not found: " + script.string();
        return false;
    }

    if (!fs::exists(venvPython)) {
        error = "Nova virtual-environment Python not found: " +
                venvPython.string();
        return false;
    }

    // Quote executable and script paths for Windows process creation.
    string command =
        "\"" + venvPython.string() + "\" \"" + script.string() + "\"";

    vector<char> mutableCommand(command.begin(), command.end());
    mutableCommand.push_back('\0');

    STARTUPINFOA startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};

    if (!CreateProcessA(
            venvPython.string().c_str(),
            mutableCommand.data(),
            nullptr, nullptr, FALSE,
            DETACHED_PROCESS | CREATE_NO_WINDOW,
            nullptr, root.string().c_str(),
            &startup, &process)) {
        error = "Could not start browser agent with " +
                venvPython.string() +
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
    if (!browserHttpRequest(L"POST", L"/command", payload.dump(), response, status)) {
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
    if (!op.resolved)
        return makeResult(op, ExecutionStatus::NOT_EXECUTED, ExecutionFailureReason::INVALID_PLAN,
            "Primitive operation is unresolved.");

    if (op.type == PrimitiveOperationType::FILESYSTEM_CREATE &&
        op.name == "filesystem.create") {
        if (!leafName(op.parameters.name))
            return fail(op, ExecutionFailureReason::UNSUPPORTED_ACTION, "Invalid folder name.", false);
        fs::path parent; ExecutionResult pathFailure;
        if (!resolvePathForOperation(op, op.parameters.location, FilesystemAccess::CREATE, parent, pathFailure)) return pathFailure;

        try {
            if (!fs::is_directory(parent)) return fail(op, ExecutionFailureReason::TARGET_NOT_FOUND,
                "Parent directory does not exist: " + parent.string());
            const fs::path folder = parent / op.parameters.name;
            if (fs::exists(folder)) return fail(op, ExecutionFailureReason::EXECUTION_ERROR,
                "Folder already exists: " + folder.string());
            if (!fs::create_directory(folder)) return fail(op, ExecutionFailureReason::EXECUTION_ERROR,
                "Folder could not be created: " + folder.string());
            return filesystemSuccess(op, "Folder created: " + folder.string(), folder);
        } catch (const fs::filesystem_error& e) {
            return fail(op, ExecutionFailureReason::EXECUTION_ERROR, e.what());
        }
    }

    if (op.type == PrimitiveOperationType::FILESYSTEM_CREATE_FILE && op.name == "filesystem.create_file") {
        if (!leafName(op.parameters.name)) return fail(op, ExecutionFailureReason::UNSUPPORTED_ACTION, "Invalid file name.", false);
        fs::path dir; ExecutionResult pathFailure;
        if (!resolvePathForOperation(op, op.parameters.location, FilesystemAccess::CREATE, dir, pathFailure)) return pathFailure;
        const fs::path filePath = dir / op.parameters.name;
        try {
            if (!fs::is_directory(dir)) return fail(op, ExecutionFailureReason::TARGET_NOT_FOUND, "Parent directory does not exist: " + dir.string());
            if (fs::exists(filePath)) return fail(op, ExecutionFailureReason::EXECUTION_ERROR, "File already exists: " + filePath.string());
            ofstream file(filePath, ios::binary);
            if (!file.is_open()) return fail(op, ExecutionFailureReason::EXECUTION_ERROR, "Unable to create file: " + filePath.string());
            file.write(op.parameters.content.data(), static_cast<streamsize>(op.parameters.content.size()));
            file.close();
            if (!file) { error_code ignored; fs::remove(filePath, ignored); return fail(op, ExecutionFailureReason::EXECUTION_ERROR, "Failed writing file: " + filePath.string()); }
            return filesystemSuccess(op, "File created: " + filePath.string(), filePath);
        } catch (const fs::filesystem_error& e) { return fail(op, ExecutionFailureReason::EXECUTION_ERROR, e.what()); }
    }

    if (op.type == PrimitiveOperationType::FILESYSTEM_COPY && op.name == "filesystem.copy") {
        fs::path source, dest; ExecutionResult pathFailure;
        if (!resolvePathForOperation(op, op.parameters.source, FilesystemAccess::DISCOVER, source, pathFailure) ||
            !resolvePathForOperation(op, op.parameters.destination, FilesystemAccess::COPY, dest, pathFailure)) return pathFailure;
        bool overwrite = false;
        if (!boolParameter(op.parameters.overwrite, false, overwrite))
            return fail(op, ExecutionFailureReason::INVALID_PLAN, "overwrite must be true/false or 1/0.", false);
        try {
            if (!fs::exists(source)) return fail(op, ExecutionFailureReason::TARGET_NOT_FOUND, "Copy source does not exist: " + source.string());
            if (fs::exists(dest) && !overwrite) return fail(op, ExecutionFailureReason::EXECUTION_ERROR, "Copy destination already exists.");
            if (!fs::is_directory(dest.parent_path())) return fail(op, ExecutionFailureReason::TARGET_NOT_FOUND, "Copy destination parent does not exist.");
            if (fs::is_directory(source)) {
                if (fs::exists(dest)) return fail(op, ExecutionFailureReason::UNSUPPORTED_ACTION, "Directory copy to an existing destination is unsupported.");
                fs::copy(source, dest, fs::copy_options::recursive);
            } else if (overwrite) fs::copy_file(source, dest, fs::copy_options::overwrite_existing);
            else fs::copy_file(source, dest);
            return filesystemSuccess(op, "Copied to: " + dest.string(), dest);
        } catch (const fs::filesystem_error& e) { return fail(op, ExecutionFailureReason::EXECUTION_ERROR, e.what()); }
    }

    if (op.type == PrimitiveOperationType::FILESYSTEM_RENAME && op.name == "filesystem.rename") {
        fs::path target; ExecutionResult pathFailure;
        if (!resolvePathForOperation(op, op.parameters.target, FilesystemAccess::RENAME, target, pathFailure) || !leafName(op.parameters.newName))
            return fail(op, ExecutionFailureReason::UNSUPPORTED_ACTION, "Rename requires an authorized target and a single-component newName.", false);
        try {
            if (!fs::exists(target)) return fail(op, ExecutionFailureReason::TARGET_NOT_FOUND, "Rename target does not exist: " + target.string());
            fs::path dest = target.parent_path() / op.parameters.newName;
            if (fs::exists(dest)) return fail(op, ExecutionFailureReason::EXECUTION_ERROR, "Rename destination already exists: " + dest.string());
            fs::rename(target, dest);
            return filesystemSuccess(op, "Renamed to: " + dest.string(), dest);
        } catch (const fs::filesystem_error& e) { return fail(op, ExecutionFailureReason::EXECUTION_ERROR, e.what()); }
    }

    if (op.type == PrimitiveOperationType::FILESYSTEM_MOVE && op.name == "filesystem.move") {
        fs::path source, dest; ExecutionResult pathFailure;
        if (!resolvePathForOperation(op, op.parameters.source, FilesystemAccess::MOVE, source, pathFailure) ||
            !resolvePathForOperation(op, op.parameters.destination, FilesystemAccess::MOVE, dest, pathFailure)) return pathFailure;
        try {
            if (!fs::exists(source)) return fail(op, ExecutionFailureReason::TARGET_NOT_FOUND, "Move source does not exist: " + source.string());
            if (fs::exists(dest)) return fail(op, ExecutionFailureReason::EXECUTION_ERROR, "Move destination already exists.");
            if (!fs::is_directory(dest.parent_path())) return fail(op, ExecutionFailureReason::TARGET_NOT_FOUND, "Move destination parent does not exist.");
            if (fs::is_directory(source)) {
                fs::path rel = dest.lexically_relative(source);
                if (!rel.empty() && *rel.begin() != "..") return fail(op, ExecutionFailureReason::UNSUPPORTED_ACTION, "Cannot move a directory inside itself.");
            }
            fs::rename(source, dest);
            return filesystemSuccess(op, "Moved to: " + dest.string(), dest);
        } catch (const fs::filesystem_error& e) { return fail(op, ExecutionFailureReason::EXECUTION_ERROR, string(e.what()) + " (rename-based move can fail across volumes)"); }
    }

    if (op.type == PrimitiveOperationType::FILESYSTEM_DELETE && op.name == "filesystem.delete") {
        fs::path target; ExecutionResult pathFailure;
        if (!resolvePathForOperation(op, op.parameters.target, FilesystemAccess::DELETE_RESOURCE, target, pathFailure)) return pathFailure;
        bool recursive = false;
        if (!boolParameter(op.parameters.recursive, false, recursive))
            return fail(op, ExecutionFailureReason::INVALID_PLAN, "recursive must be true/false or 1/0.", false);
        try {
            if (!fs::exists(target)) return fail(op, ExecutionFailureReason::TARGET_NOT_FOUND, "Delete target does not exist: " + target.string());
            if (fs::is_directory(target)) {
                if (!recursive && !fs::is_empty(target)) return fail(op, ExecutionFailureReason::EXECUTION_ERROR, "Directory is not empty; recursive deletion was not enabled.");
                if (recursive) fs::remove_all(target); else fs::remove(target);
            } else fs::remove(target);
            return filesystemSuccess(op, "Deleted: " + target.string(), target);
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
            if (!fs::is_directory(directory)) return fail(op, ExecutionFailureReason::TARGET_NOT_FOUND, "Directory does not exist: " + directory.string());
            vector<string> entries;
            for (const auto& entry : fs::directory_iterator(directory, fs::directory_options::skip_permission_denied)) {
                entries.push_back(entry.path().string());
                if (static_cast<int>(entries.size()) == limit) break;
            }
            return filesystemSuccess(op, "Listed " + to_string(entries.size()) + " entries in: " + directory.string(), directory, entries);
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
            if (!fs::is_directory(root)) return fail(op, ExecutionFailureReason::TARGET_NOT_FOUND, "Search root does not exist: " + root.string());
            vector<string> matches;
            const string needle = lower(op.parameters.query);
            error_code iteratorError;
            fs::recursive_directory_iterator iterator(root,
                fs::directory_options::skip_permission_denied, iteratorError);
            const fs::recursive_directory_iterator end;
            while (!iteratorError && iterator != end) {
                const fs::directory_entry entry = *iterator;
                error_code typeError;
                const bool isFile = entry.is_regular_file(typeError);
                typeError.clear();
                const bool isDirectory = entry.is_directory(typeError);
                const bool acceptedType = !typeError && (type == "any" ||
                    (type == "file" && isFile) || (type == "directory" && isDirectory));
                if (acceptedType && lower(entry.path().filename().string()).find(needle) != string::npos) {
                    matches.push_back(entry.path().string());
                    if (static_cast<int>(matches.size()) == limit) break;
                }
                iterator.increment(iteratorError);
                if (iteratorError) iteratorError.clear(); // skip an unreadable child and continue.
            }
            return filesystemSuccess(op, "Found " + to_string(matches.size()) + " matching entries under: " + root.string(), root, matches);
        } catch (const fs::filesystem_error& e) { return fail(op, ExecutionFailureReason::EXECUTION_ERROR, e.what()); }
    }

    if (op.type == PrimitiveOperationType::FILESYSTEM_INSPECT && op.name == "filesystem.inspect") {
        fs::path target; ExecutionResult pathFailure;
        if (!resolvePathForOperation(op, op.parameters.target, FilesystemAccess::DISCOVER, target, pathFailure)) return pathFailure;
        try {
            if (!fs::exists(target)) return fail(op, ExecutionFailureReason::TARGET_NOT_FOUND, "Path does not exist: " + target.string());
            const string kind = fs::is_directory(target) ? "directory" : (fs::is_regular_file(target) ? "file" : "other");
            return filesystemSuccess(op, "Inspected " + kind + ": " + target.string(), target, {target.string()});
        } catch (const fs::filesystem_error& e) { return fail(op, ExecutionFailureReason::EXECUTION_ERROR, e.what()); }
    }

    
    if (op.name == "browser.observe" || op.name == "browser.click" || op.name == "browser.type") {
        nlohmann::json request;
        if (op.name == "browser.observe") {
            request = {{"command", "observe"}};
        } else {
            const bool typing = op.name == "browser.type";
            if (op.parameters.elementId.empty() &&
                (op.parameters.role.empty() || op.parameters.text.empty())) {
                return fail(op, ExecutionFailureReason::INVALID_PLAN,
                    "Browser action requires element_id or both role and text.", false);
            }
            request = {{"command", typing ? "type" : "click"},
                       {"element_id", op.parameters.elementId},
                       {"role", op.parameters.role},
                       {"text", op.parameters.text}};
            if (typing) request["content"] = op.parameters.content;
        }
        nlohmann::json response;
        string error;
        if (!browserAgentRequest(request, response, error))
            return fail(op, ExecutionFailureReason::EXECUTION_ERROR, "Browser control failed: " + error);
        string message = response.value("message", string("Browser operation completed."));
        // Include the structured DOM snapshot for every browser primitive. Click/type
        // responses contain a fresh post-action observation, so later planning is based
        // on actual page state rather than site-specific success assumptions.
        message += "\nBrowser DOM result: " + response.dump();
        return makeResult(op, ExecutionStatus::SUCCESS, ExecutionFailureReason::NONE,
            message, true, true);
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
        if (!browserAgentRequest(request, response, error))
            return fail(op, ExecutionFailureReason::EXECUTION_ERROR,
                "Browser control failed: " + error);
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

    if (op.type == PrimitiveOperationType::APPLICATION_LAUNCH &&
        op.name == "application.launch") {

        if (op.parameters.application.empty()) {
            return fail(
                op,
                ExecutionFailureReason::UNSUPPORTED_ACTION,
                "Application name is missing.",
                false
            );
        }

        const string requestedApp = lower(op.parameters.application);
        if (op.parameters.target.empty() &&
            (requestedApp == "edge" || requestedApp == "microsoft edge" || requestedApp == "msedge.exe" ||
             requestedApp == "chrome" || requestedApp == "google chrome" || requestedApp == "chrome.exe")) {
            nlohmann::json browserRequest = {
                {"command", "launch"},
                {"browser", (requestedApp == "chrome" || requestedApp == "google chrome" || requestedApp == "chrome.exe")
                    ? "chrome" : "msedge"}
            };
            nlohmann::json browserResponse;
            string browserError;
            if (!browserAgentRequest(browserRequest, browserResponse, browserError))
                return fail(op, ExecutionFailureReason::EXECUTION_ERROR,
                    "Could not launch the controlled browser: " + browserError);
            return makeResult(op, ExecutionStatus::SUCCESS, ExecutionFailureReason::NONE,
                browserResponse.value("message", string("Controlled browser launched.")), true, true);
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

        const string shellTarget =
            "shell:AppsFolder\\" + resolved.appId;

        HINSTANCE launchResult = ShellExecuteA(
            nullptr,
            "open",
            shellTarget.c_str(),
            nullptr,
            nullptr,
            SW_SHOWNORMAL
        );

        if (reinterpret_cast<INT_PTR>(launchResult) <= 32) {
            return fail(
                op,
                ExecutionFailureReason::EXECUTION_ERROR,
                "Windows failed to launch application: " +
                    resolved.displayName,
                false
            );
        }

        return makeResult(
            op,
            ExecutionStatus::SUCCESS,
            ExecutionFailureReason::NONE,
            "Launch request accepted for " + resolved.displayName + ".",
            true,
            true
        );
    }


    if (op.type == PrimitiveOperationType::APPLICATION_FOCUS && op.name == "application.focus") {
        const string exe = appExeFromName(op.parameters.application);
        if (exe.empty())
            return fail(op, ExecutionFailureReason::UNSUPPORTED_ACTION,
                "Application is not in the supported allowlist: " + op.parameters.application, false);

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
                if (GetForegroundWindow() == hwnd)
                    return makeResult(op, ExecutionStatus::SUCCESS, ExecutionFailureReason::NONE,
                        "Focused " + op.parameters.application + " window (PID " + to_string(pid) + ").", true, true);
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
                "Application is not in the supported allowlist: " + op.parameters.application, false);

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
                return makeResult(op, ExecutionStatus::SUCCESS, ExecutionFailureReason::NONE,
                    "Close request sent to " + op.parameters.application +
                    " window (PID " + to_string(pid) + ").", true, true);
            }
            return fail(op, ExecutionFailureReason::EXECUTION_ERROR, "Failed to send WM_CLOSE to target window.");
        }
        return fail(op, ExecutionFailureReason::TARGET_NOT_FOUND,
            "No visible window found to close for " + op.parameters.application +
            (title.empty() ? string{} : " matching title: " + title));
    }


    if (op.type == PrimitiveOperationType::TEXT_WRITE && op.name == "text.write") {
        fs::path target; ExecutionResult pathFailure;
        if (!resolvePathForOperation(op, op.parameters.target, FilesystemAccess::WRITE, target, pathFailure)) return pathFailure;
        try {
            if (!fs::is_regular_file(target))
                return fail(op, ExecutionFailureReason::TARGET_NOT_FOUND,
                    "text.write target must be an existing regular file: " + target.string());
            ofstream file(target, ios::binary | ios::trunc);
            if (!file.is_open())
                return fail(op, ExecutionFailureReason::EXECUTION_ERROR,
                    "Unable to open target for writing: " + target.string());
            file.write(op.parameters.content.data(),
                static_cast<streamsize>(op.parameters.content.size()));
            file.close();
            if (!file)
                return fail(op, ExecutionFailureReason::EXECUTION_ERROR,
                    "Failed while writing target: " + target.string());
            return filesystemSuccess(op, "Text written to file: " + target.string(), target);
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
