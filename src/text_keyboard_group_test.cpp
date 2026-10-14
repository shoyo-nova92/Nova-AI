#include <windows.h>
#include <shlobj.h>
#include "primitive_execution_engine.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <thread>

using namespace std;
namespace fs = std::filesystem;
namespace {
int passed = 0, failed = 0;
void check(const string& name, bool ok) {
    cout << "[" << name << "] " << (ok ? "PASS" : "FAIL") << '\n';
    (ok ? passed : failed)++;
}
fs::path desktop() {
    PWSTR raw = nullptr;
    HRESULT hr = SHGetKnownFolderPath(FOLDERID_Desktop, 0, nullptr, &raw);
    if (FAILED(hr) || !raw) return {};
    fs::path p(raw); CoTaskMemFree(raw); return p;
}
PrimitiveOperation base(PrimitiveOperationType type, const string& name) {
    PrimitiveOperation p{}; p.type = type; p.name = name; p.resolved = true; return p;
}
PrimitiveOperation createFolder(const string& name) {
    auto p = base(PrimitiveOperationType::FILESYSTEM_CREATE, "filesystem.create");
    p.parameters.name = name; p.parameters.location = "desktop"; return p;
}
PrimitiveOperation createFile(const string& name, const string& location, const string& content) {
    auto p = base(PrimitiveOperationType::FILESYSTEM_CREATE_FILE, "filesystem.create_file");
    p.parameters.name = name; p.parameters.location = location; p.parameters.content = content; return p;
}
PrimitiveOperation textWrite(const string& target, const string& content) {
    auto p = base(PrimitiveOperationType::TEXT_WRITE, "text.write");
    p.parameters.target = target; p.parameters.content = content; return p;
}
PrimitiveOperation keyPress(const string& key, const string& modifiers = "", const string& repeat = "") {
    auto p = base(PrimitiveOperationType::KEYBOARD_PRESS, "keyboard.press");
    p.parameters.key = key; p.parameters.modifiers = modifiers; p.parameters.repeatCount = repeat; return p;
}
PrimitiveOperation keyType(const string& content, const string& target = "") {
    auto p = base(PrimitiveOperationType::KEYBOARD_TYPE, "keyboard.type");
    p.parameters.content = content; p.parameters.target = target; return p;
}
string contents(const fs::path& p) {
    ifstream f(p, ios::binary);
    return string(istreambuf_iterator<char>(f), istreambuf_iterator<char>());
}
}
int main() {
    cout << "=== GROUP 4: TEXT AND KEYBOARD TEST ===\n";
    const fs::path desk = desktop();
    if (desk.empty() || !fs::is_directory(desk)) { cerr << "Desktop unavailable.\n"; return 2; }
    const string unique = "NovaGroup4Test_" +
        to_string(chrono::steady_clock::now().time_since_epoch().count());
    const fs::path workspace = desk / unique;
    const string rel = "desktop/" + unique;
    if (fs::exists(workspace)) { cerr << "Unique test path collision; abort.\n"; return 2; }

    PrimitiveExecutionEngine engine;
    bool created = false;
    auto run = [&](const string& label, const PrimitiveOperation& p, ExecutionStatus expected) {
        auto r = engine.execute(p);
        bool ok = r.status == expected;
        check(label, ok);
        if (!ok) cerr << "  status=" << static_cast<int>(r.status)
            << " reason=" << static_cast<int>(r.reason) << " message=" << r.message << '\n';
        return r;
    };

    try {
        auto r = run("01_CREATE_WORKSPACE", createFolder(unique), ExecutionStatus::SUCCESS);
        created = r.status == ExecutionStatus::SUCCESS && fs::is_directory(workspace);
        check("02_WORKSPACE_EXISTS", created);
        if (!created) throw runtime_error("Workspace creation failed.");

        const string initial = "Initial test contents.\n";
        r = run("03_CREATE_TEXT_FILE",
            createFile("Nova_Text_Test.txt", rel, initial), ExecutionStatus::SUCCESS);
        const fs::path file = workspace / "Nova_Text_Test.txt";
        check("04_INITIAL_CONTENT_VERIFIED",
            r.status == ExecutionStatus::SUCCESS && fs::is_regular_file(file) && contents(file) == initial);

        const string replacement = "Text written by Nova text.write.\nUnicode check: cafe.\n";
        r = run("05_TEXT_WRITE_REPLACES_TEST_FILE",
            textWrite(rel + "/Nova_Text_Test.txt", replacement), ExecutionStatus::SUCCESS);
        check("06_TEXT_WRITE_CONTENT_EXACT",
            r.status == ExecutionStatus::SUCCESS && contents(file) == replacement);

        run("07_TEXT_WRITE_TRAVERSAL_REJECTED",
            textWrite("desktop/../outside.txt", "unsafe"), ExecutionStatus::NOT_EXECUTED);
        run("08_UNKNOWN_KEY_REJECTED",
            keyPress("NOT_A_REAL_KEY"), ExecutionStatus::NOT_EXECUTED);
        run("09_INVALID_REPEAT_REJECTED",
            keyPress("ENTER", "", "101"), ExecutionStatus::NOT_EXECUTED);
        run("10_INVALID_MODIFIER_REJECTED",
            keyPress("A", "HYPER"), ExecutionStatus::NOT_EXECUTED);

        // Open the test file in Notepad; keyboard input is sent only after matching its title.
        auto launch = base(PrimitiveOperationType::APPLICATION_LAUNCH, "application.launch");
        launch.parameters.application = "notepad";
        launch.parameters.target = rel + "/Nova_Text_Test.txt";
        r = run("11_LAUNCH_NOTEPAD_TEST_FILE", launch, ExecutionStatus::SUCCESS);
        if (r.status == ExecutionStatus::SUCCESS) {
            this_thread::sleep_for(chrono::milliseconds(900));
            // Select all, replace the file contents through real keyboard input, then save.
            auto focus = base(PrimitiveOperationType::APPLICATION_FOCUS, "application.focus");
            focus.parameters.application = "notepad";
            focus.parameters.target = "Nova_Text_Test.txt";
            auto f = engine.execute(focus);
            check("12_FOCUS_NOTEPAD_OUTCOME",
                f.status == ExecutionStatus::SUCCESS ||
                f.reason == ExecutionFailureReason::EXECUTION_ERROR);
            auto selectAll = run("13_PRESS_CTRL_A",
                keyPress("A", "CTRL"), ExecutionStatus::SUCCESS);
            auto typed = run("14_KEYBOARD_TYPE_TO_TEST_WINDOW",
                keyType("Keyboard input delivered by Nova.\n", "Nova_Text_Test.txt"),
                ExecutionStatus::SUCCESS);
            auto save = run("15_PRESS_CTRL_S",
                keyPress("S", "CTRL"), ExecutionStatus::SUCCESS);
            this_thread::sleep_for(chrono::milliseconds(700));
            check("16_KEYBOARD_OPERATION_MESSAGES_ACCEPTED",
                selectAll.status == ExecutionStatus::SUCCESS &&
                typed.status == ExecutionStatus::SUCCESS &&
                save.status == ExecutionStatus::SUCCESS);
            auto close = base(PrimitiveOperationType::APPLICATION_CLOSE, "application.close");
            close.parameters.application = "notepad";
            close.parameters.target = "Nova_Text_Test.txt";
            auto closeResult = engine.execute(close);
            check("17_CLOSE_TEST_NOTEPAD_OUTCOME",
                closeResult.status == ExecutionStatus::SUCCESS ||
                closeResult.reason == ExecutionFailureReason::TARGET_NOT_FOUND ||
                closeResult.reason == ExecutionFailureReason::EXECUTION_ERROR);
        } else {
            check("12_FOCUS_NOTEPAD_OUTCOME", false);
            check("13_PRESS_CTRL_A", false);
            check("14_KEYBOARD_TYPE_TO_TEST_WINDOW", false);
            check("15_PRESS_CTRL_S", false);
            check("16_KEYBOARD_OPERATION_MESSAGES_ACCEPTED", false);
            check("17_CLOSE_TEST_NOTEPAD_OUTCOME", false);
        }

        auto unresolved = keyPress("ENTER");
        unresolved.resolved = false;
        run("18_UNRESOLVED_KEY_OPERATION_REJECTED", unresolved, ExecutionStatus::NOT_EXECUTED);
    } catch (const exception& e) {
        cerr << "TEST ABORTED: " << e.what() << '\n';
        ++failed;
    }

    error_code ec;
    if (created && fs::exists(workspace)) fs::remove_all(workspace, ec);
    check("19_TEST_WORKSPACE_CLEANED", !fs::exists(workspace) && !ec);
    cout << "\nGROUP 4 TEXT AND KEYBOARD TEST\nPassed: " << passed
         << "\nFailed: " << failed << '\n'
         << (failed == 0 ? "RESULT: PASS\n" : "RESULT: FAIL\n");
    return failed == 0 ? 0 : 1;
}
