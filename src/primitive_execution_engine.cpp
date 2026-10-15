#include <windows.h>
#include <psapi.h>
#include <shlobj.h>
#include "primitive_execution_engine.hpp"

#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>
#include <algorithm>
#include <cctype>
#include <sstream>
#include "application_resolver.hpp"

using namespace std;
namespace fs = std::filesystem;

namespace {
fs::path getDesktopPath() {
    PWSTR raw = nullptr;
    HRESULT hr = SHGetKnownFolderPath(FOLDERID_Desktop, 0, nullptr, &raw);
    if (FAILED(hr) || raw == nullptr) return {};
    fs::path path(raw);
    CoTaskMemFree(raw);
    return path;
}

ExecutionResult makeResult(const PrimitiveOperation& op, ExecutionStatus status,
    ExecutionFailureReason reason, const string& message,
    bool targetResolved = false, bool mechanismResolved = false) {
    ExecutionResult r{};
    r.status = status;
    r.reason = reason;
    r.action = op.name;
    r.message = message;
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

bool desktopPath(const string& text, const fs::path& desktop, fs::path& result,
    bool allowDesktopRoot = false) {
    if (desktop.empty() || text.empty()) return false;
    if (text == "desktop") {
        if (!allowDesktopRoot) return false;
        result = desktop.lexically_normal();
        return true;
    }
    const string prefix = "desktop/";
    if (text.rfind(prefix, 0) != 0) return false;
    fs::path rel(text.substr(8));
    if (rel.empty() || rel.is_absolute() || rel.has_root_name() || rel.has_root_directory()) return false;
    for (const auto& part : rel) {
        const string s = part.string();
        if (s.empty() || s == "." || s == "..") return false;
    }
    fs::path base = desktop.lexically_normal();
    fs::path candidate = (base / rel).lexically_normal();
    auto b = base.begin(); auto c = candidate.begin();
    for (; b != base.end(); ++b, ++c)
        if (c == candidate.end() || *b != *c) return false;
    result = candidate;
    return true;
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
    const fs::path desktop = getDesktopPath();
    if (!desktopPath(target, desktop, resolved, true)) return false;
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

    const fs::path desktop = getDesktopPath();

    
    if (op.type == PrimitiveOperationType::FILESYSTEM_CREATE &&
        op.name == "filesystem.create") {

        if (!leafName(op.parameters.name)) {
            return fail(op, ExecutionFailureReason::UNSUPPORTED_ACTION,
                "Invalid folder name.", false);
        }

        fs::path parent;
        if (!desktopPath(op.parameters.location, desktop, parent, true)) {
            return fail(op, ExecutionFailureReason::UNSUPPORTED_ACTION,
                "Unsupported or unsafe folder location.", false);
        }

        try {
            if (!fs::is_directory(parent)) {
                return fail(op, ExecutionFailureReason::TARGET_NOT_FOUND,
                    "Parent directory does not exist: " + parent.string());
            }

            const fs::path folder = parent / op.parameters.name;

            if (fs::exists(folder)) {
                return fail(op, ExecutionFailureReason::EXECUTION_ERROR,
                    "Folder already exists: " + folder.string());
            }

            if (!fs::create_directory(folder)) {
                return fail(op, ExecutionFailureReason::EXECUTION_ERROR,
                    "Folder could not be created: " + folder.string());
            }

            return makeResult(op, ExecutionStatus::SUCCESS,
                ExecutionFailureReason::NONE,
                "Folder created: " + folder.string(), true, true);

        } catch (const fs::filesystem_error& e) {
            return fail(op, ExecutionFailureReason::EXECUTION_ERROR, e.what());
        }
    }


    if (op.type == PrimitiveOperationType::FILESYSTEM_CREATE_FILE && op.name == "filesystem.create_file") {
        if (!leafName(op.parameters.name)) return fail(op, ExecutionFailureReason::UNSUPPORTED_ACTION, "Invalid file name.", false);
        fs::path dir;
        if (!desktopPath(op.parameters.location, desktop, dir, true))
            return fail(op, ExecutionFailureReason::UNSUPPORTED_ACTION, "Unsupported or unsafe file location.", false);
        const fs::path filePath = dir / op.parameters.name;
        try {
            if (!fs::is_directory(dir)) return fail(op, ExecutionFailureReason::TARGET_NOT_FOUND, "Parent directory does not exist: " + dir.string());
            if (fs::exists(filePath)) return fail(op, ExecutionFailureReason::EXECUTION_ERROR, "File already exists: " + filePath.string());
            ofstream file(filePath, ios::binary);
            if (!file.is_open()) return fail(op, ExecutionFailureReason::EXECUTION_ERROR, "Unable to create file: " + filePath.string());
            file.write(op.parameters.content.data(), static_cast<streamsize>(op.parameters.content.size()));
            file.close();
            if (!file) { error_code ignored; fs::remove(filePath, ignored); return fail(op, ExecutionFailureReason::EXECUTION_ERROR, "Failed writing file: " + filePath.string()); }
            return makeResult(op, ExecutionStatus::SUCCESS, ExecutionFailureReason::NONE, "File created: " + filePath.string(), true, true);
        } catch (const fs::filesystem_error& e) { return fail(op, ExecutionFailureReason::EXECUTION_ERROR, e.what()); }
    }

    if (op.type == PrimitiveOperationType::FILESYSTEM_COPY && op.name == "filesystem.copy") {
        fs::path source, dest;
        if (!desktopPath(op.parameters.source, desktop, source) || !desktopPath(op.parameters.destination, desktop, dest))
            return fail(op, ExecutionFailureReason::UNSUPPORTED_ACTION, "Copy requires safe desktop-relative paths.", false);
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
            return makeResult(op, ExecutionStatus::SUCCESS, ExecutionFailureReason::NONE, "Copied to: " + dest.string(), true, true);
        } catch (const fs::filesystem_error& e) { return fail(op, ExecutionFailureReason::EXECUTION_ERROR, e.what()); }
    }

    if (op.type == PrimitiveOperationType::FILESYSTEM_RENAME && op.name == "filesystem.rename") {
        fs::path target;
        if (!desktopPath(op.parameters.target, desktop, target) || !leafName(op.parameters.newName))
            return fail(op, ExecutionFailureReason::UNSUPPORTED_ACTION, "Rename requires a safe target and a single-component newName.", false);
        try {
            if (!fs::exists(target)) return fail(op, ExecutionFailureReason::TARGET_NOT_FOUND, "Rename target does not exist: " + target.string());
            fs::path dest = target.parent_path() / op.parameters.newName;
            if (fs::exists(dest)) return fail(op, ExecutionFailureReason::EXECUTION_ERROR, "Rename destination already exists: " + dest.string());
            fs::rename(target, dest);
            return makeResult(op, ExecutionStatus::SUCCESS, ExecutionFailureReason::NONE, "Renamed to: " + dest.string(), true, true);
        } catch (const fs::filesystem_error& e) { return fail(op, ExecutionFailureReason::EXECUTION_ERROR, e.what()); }
    }

    if (op.type == PrimitiveOperationType::FILESYSTEM_MOVE && op.name == "filesystem.move") {
        fs::path source, dest;
        if (!desktopPath(op.parameters.source, desktop, source) || !desktopPath(op.parameters.destination, desktop, dest))
            return fail(op, ExecutionFailureReason::UNSUPPORTED_ACTION, "Move requires safe desktop-relative paths.", false);
        try {
            if (!fs::exists(source)) return fail(op, ExecutionFailureReason::TARGET_NOT_FOUND, "Move source does not exist: " + source.string());
            if (fs::exists(dest)) return fail(op, ExecutionFailureReason::EXECUTION_ERROR, "Move destination already exists.");
            if (!fs::is_directory(dest.parent_path())) return fail(op, ExecutionFailureReason::TARGET_NOT_FOUND, "Move destination parent does not exist.");
            if (fs::is_directory(source)) {
                fs::path rel = dest.lexically_relative(source);
                if (!rel.empty() && *rel.begin() != "..") return fail(op, ExecutionFailureReason::UNSUPPORTED_ACTION, "Cannot move a directory inside itself.");
            }
            fs::rename(source, dest);
            return makeResult(op, ExecutionStatus::SUCCESS, ExecutionFailureReason::NONE, "Moved to: " + dest.string(), true, true);
        } catch (const fs::filesystem_error& e) { return fail(op, ExecutionFailureReason::EXECUTION_ERROR, string(e.what()) + " (rename-based move can fail across volumes)"); }
    }

    if (op.type == PrimitiveOperationType::FILESYSTEM_DELETE && op.name == "filesystem.delete") {
        fs::path target;
        if (!desktopPath(op.parameters.target, desktop, target))
            return fail(op, ExecutionFailureReason::UNSUPPORTED_ACTION, "Delete requires a safe desktop-relative target.", false);
        bool recursive = false;
        if (!boolParameter(op.parameters.recursive, false, recursive))
            return fail(op, ExecutionFailureReason::INVALID_PLAN, "recursive must be true/false or 1/0.", false);
        try {
            if (!fs::exists(target)) return fail(op, ExecutionFailureReason::TARGET_NOT_FOUND, "Delete target does not exist: " + target.string());
            if (fs::is_directory(target)) {
                if (!recursive && !fs::is_empty(target)) return fail(op, ExecutionFailureReason::EXECUTION_ERROR, "Directory is not empty; recursive deletion was not enabled.");
                if (recursive) fs::remove_all(target); else fs::remove(target);
            } else fs::remove(target);
            return makeResult(op, ExecutionStatus::SUCCESS, ExecutionFailureReason::NONE, "Deleted: " + target.string(), true, true);
        } catch (const fs::filesystem_error& e) { return fail(op, ExecutionFailureReason::EXECUTION_ERROR, e.what()); }
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

        fs::path target;

        if (!op.parameters.target.empty()) {
            if (!launchTargetIsSafe(op.parameters.target, target)) {
                return fail(
                    op,
                    ExecutionFailureReason::UNSUPPORTED_ACTION,
                    "Launch target must be an existing Desktop-relative path.",
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
        fs::path target;
        if (!desktopPath(op.parameters.target, desktop, target))
            return fail(op, ExecutionFailureReason::UNSUPPORTED_ACTION,
                "text.write requires a safe desktop-relative file path.", false);
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
            return makeResult(op, ExecutionStatus::SUCCESS, ExecutionFailureReason::NONE,
                "Text written to file: " + target.string(), true, true);
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
