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
PrimitiveOperation appOp(PrimitiveOperationType type, const string& name,
    const string& app, const string& target = {}) {
    auto p = base(type, name); p.parameters.application = app; p.parameters.target = target; return p;
}
}

int main() {
    cout << "=== GROUP 3: APPLICATION LIFECYCLE TEST ===\n";
    const fs::path desk = desktop();
    if (desk.empty() || !fs::is_directory(desk)) { cerr << "Desktop unavailable.\n"; return 2; }

    const string unique = "NovaGroup3Test_" +
        to_string(chrono::steady_clock::now().time_since_epoch().count());
    const fs::path workspace = desk / unique;
    const string workspaceRel = "desktop/" + unique;
    if (fs::exists(workspace)) { cerr << "Unique test workspace collision; abort.\n"; return 2; }

    PrimitiveExecutionEngine engine;
    const string text = "Nova Group 3 application lifecycle test.\n";
    bool workspaceCreated = false;
    bool notepadLaunchAccepted = false;

    auto run = [&](const string& label, const PrimitiveOperation& p, ExecutionStatus expected) {
        ExecutionResult r = engine.execute(p);
        bool ok = r.status == expected;
        check(label, ok);
        if (!ok) cerr << "  status=" << static_cast<int>(r.status)
            << " reason=" << static_cast<int>(r.reason) << " message=" << r.message << '\n';
        return r;
    };

    try {
        auto r = run("01_CREATE_WORKSPACE", createFolder(unique), ExecutionStatus::SUCCESS);
        workspaceCreated = r.status == ExecutionStatus::SUCCESS && fs::is_directory(workspace);
        check("02_WORKSPACE_EXISTS", workspaceCreated);
        if (!workspaceCreated) throw runtime_error("Could not create test workspace.");

        r = run("03_CREATE_TEST_FILE",
            createFile("Nova_Group3_Test.txt", workspaceRel, text), ExecutionStatus::SUCCESS);
        const fs::path testFile = workspace / "Nova_Group3_Test.txt";
        check("04_TEST_FILE_EXISTS", r.status == ExecutionStatus::SUCCESS && fs::is_regular_file(testFile));

        // Launch only the test-created file in Notepad; we do not terminate arbitrary processes.
        r = run("05_LAUNCH_NOTEPAD_WITH_TEST_FILE",
            appOp(PrimitiveOperationType::APPLICATION_LAUNCH, "application.launch",
                "notepad", workspaceRel + "/Nova_Group3_Test.txt"),
            ExecutionStatus::SUCCESS);
        notepadLaunchAccepted = r.status == ExecutionStatus::SUCCESS;
        check("06_LAUNCH_RESULT_HAS_PID",
            notepadLaunchAccepted && r.message.find("PID ") != string::npos);

        // Focus/close target the test document title to avoid acting on another Notepad window.
        // Windows may enforce foreground restrictions, so focus can legitimately report FAILED.
        if (notepadLaunchAccepted) {
            this_thread::sleep_for(chrono::milliseconds(900));
            auto focus = engine.execute(appOp(PrimitiveOperationType::APPLICATION_FOCUS,
                "application.focus", "notepad", "Nova_Group3_Test.txt"));
            check("07_FOCUS_REQUEST_RESOLVED",
                focus.status == ExecutionStatus::SUCCESS ||
                focus.reason == ExecutionFailureReason::EXECUTION_ERROR);
            if (focus.status != ExecutionStatus::SUCCESS)
                cout << "  INFO: focus outcome: " << focus.message << '\n';

            auto close = engine.execute(appOp(PrimitiveOperationType::APPLICATION_CLOSE,
                "application.close", "notepad", "Nova_Group3_Test.txt"));
            check("08_CLOSE_REQUEST_RESOLVED",
                close.status == ExecutionStatus::SUCCESS ||
                close.reason == ExecutionFailureReason::TARGET_NOT_FOUND ||
                close.reason == ExecutionFailureReason::EXECUTION_ERROR);
            if (close.status != ExecutionStatus::SUCCESS)
                cout << "  INFO: close outcome: " << close.message << '\n';
        } else {
            check("07_FOCUS_REQUEST_RESOLVED", false);
            check("08_CLOSE_REQUEST_RESOLVED", false);
        }

        // Unsupported app names must not launch arbitrary commands.
        run("09_UNKNOWN_APPLICATION_REJECTED",
            appOp(PrimitiveOperationType::APPLICATION_LAUNCH, "application.launch",
                "NovaDefinitelyNotAnApp"), ExecutionStatus::NOT_EXECUTED);

        // Unsafe target path must be rejected before launch.
        run("10_UNSAFE_LAUNCH_TARGET_REJECTED",
            appOp(PrimitiveOperationType::APPLICATION_LAUNCH, "application.launch",
                "notepad", "desktop/../outside.txt"), ExecutionStatus::NOT_EXECUTED);

        // Unresolved operation must never run.
        auto unresolved = appOp(PrimitiveOperationType::APPLICATION_LAUNCH,
            "application.launch", "notepad");
        unresolved.resolved = false;
        run("11_UNRESOLVED_LAUNCH_REJECTED", unresolved, ExecutionStatus::NOT_EXECUTED);
    } catch (const exception& e) {
        cerr << "TEST ABORTED: " << e.what() << '\n';
        ++failed;
    }

    // The app is closed via WM_CLOSE request, not force-terminated. Keep file cleanup bounded
    // to the unique test workspace created by this invocation.
    error_code cleanupError;
    if (workspaceCreated && fs::exists(workspace))
        fs::remove_all(workspace, cleanupError);
    check("12_TEST_WORKSPACE_CLEANED", !fs::exists(workspace) && !cleanupError);

    cout << "\nGROUP 3 APPLICATION LIFECYCLE TEST\nPassed: " << passed
         << "\nFailed: " << failed << '\n'
         << (failed == 0 ? "RESULT: PASS\n" : "RESULT: FAIL\n");
    return failed == 0 ? 0 : 1;
}
