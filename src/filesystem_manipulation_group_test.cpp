#include <windows.h>
#include <shlobj.h>
#include "primitive_execution_engine.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <stdexcept>

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
PrimitiveOperation folder(const string& name) {
    auto p = base(PrimitiveOperationType::FILESYSTEM_CREATE, "filesystem.create");
    p.parameters.name = name; p.parameters.location = "desktop"; return p;
}
PrimitiveOperation file(const string& name, const string& location, const string& content) {
    auto p = base(PrimitiveOperationType::FILESYSTEM_CREATE_FILE, "filesystem.create_file");
    p.parameters.name = name; p.parameters.location = location; p.parameters.content = content; return p;
}
PrimitiveOperation copyFile(const string& source, const string& destination, const string& overwrite = "") {
    auto p = base(PrimitiveOperationType::FILESYSTEM_COPY, "filesystem.copy");
    p.parameters.source = source; p.parameters.destination = destination; p.parameters.overwrite = overwrite; return p;
}
PrimitiveOperation renameFile(const string& target, const string& newName) {
    auto p = base(PrimitiveOperationType::FILESYSTEM_RENAME, "filesystem.rename");
    p.parameters.target = target; p.parameters.newName = newName; return p;
}
PrimitiveOperation moveFile(const string& source, const string& destination) {
    auto p = base(PrimitiveOperationType::FILESYSTEM_MOVE, "filesystem.move");
    p.parameters.source = source; p.parameters.destination = destination; return p;
}
PrimitiveOperation deleteResource(const string& target, const string& recursive = "") {
    auto p = base(PrimitiveOperationType::FILESYSTEM_DELETE, "filesystem.delete");
    p.parameters.target = target; p.parameters.recursive = recursive; return p;
}
string contents(const fs::path& p) {
    ifstream f(p, ios::binary);
    return string(istreambuf_iterator<char>(f), istreambuf_iterator<char>());
}
}

int main() {
    cout << "=== GROUP 2: CHAINED FILESYSTEM INTEGRATION TEST ===\n";
    const fs::path desk = desktop();
    if (desk.empty() || !fs::is_directory(desk)) { cerr << "Desktop unavailable.\n"; return 2; }

    const string unique = "NovaGroup2Test_" +
        to_string(chrono::steady_clock::now().time_since_epoch().count());
    const string stagingName = unique + "_staging";
    const fs::path root = desk / unique, staging = desk / stagingName;
    const string rootRel = "desktop/" + unique, stagingRel = "desktop/" + stagingName;
    if (fs::exists(root) || fs::exists(staging)) { cerr << "Unique test path collision; abort.\n"; return 2; }

    PrimitiveExecutionEngine engine;
    const string originalText = "public class Main { public static void main(String[] args) {} }\n";
    bool rootCreated = false, stagingCreated = false;

    auto run = [&](const string& label, const PrimitiveOperation& p, ExecutionStatus expected) {
        ExecutionResult r = engine.execute(p);
        bool ok = r.status == expected;
        check(label, ok);
        if (!ok) cerr << "  status=" << static_cast<int>(r.status)
            << " reason=" << static_cast<int>(r.reason) << " message=" << r.message << '\n';
        return r;
    };

    try {
        auto r = run("01_CREATE_WORKSPACE", folder(unique), ExecutionStatus::SUCCESS);
        rootCreated = r.status == ExecutionStatus::SUCCESS && fs::is_directory(root);
        check("02_WORKSPACE_EXISTS", rootCreated);
        if (!rootCreated) throw runtime_error("workspace creation failed");

        r = run("03_CREATE_MAIN_JAVA", file("Main.java", rootRel, originalText), ExecutionStatus::SUCCESS);
        const fs::path mainFile = root / "Main.java";
        check("04_MAIN_JAVA_CONTENT_EXACT", r.status == ExecutionStatus::SUCCESS &&
            fs::is_regular_file(mainFile) && contents(mainFile) == originalText);

        const string copyRel = rootRel + "/Main_copy.java";
        run("05_COPY_MAIN_JAVA", copyFile(rootRel + "/Main.java", copyRel), ExecutionStatus::SUCCESS);
        const fs::path copied = root / "Main_copy.java";
        check("06_COPY_BYTES_MATCH", fs::is_regular_file(copied) && contents(copied) == originalText);
        run("07_DUPLICATE_COPY_REJECTED", copyFile(rootRel + "/Main.java", copyRel), ExecutionStatus::FAILED);
        check("08_DUPLICATE_COPY_PRESERVED", fs::is_regular_file(copied) && contents(copied) == originalText);
        run("09_MISSING_COPY_SOURCE_REJECTED",
            copyFile(rootRel + "/missing.java", rootRel + "/missing_copy.java"), ExecutionStatus::FAILED);
        check("10_MISSING_COPY_DESTINATION_ABSENT", !fs::exists(root / "missing_copy.java"));

        run("11_RENAME_COPY", renameFile(copyRel, "Main_renamed.java"), ExecutionStatus::SUCCESS);
        const fs::path renamed = root / "Main_renamed.java";
        check("12_RENAMED_FILE_CONTENT", fs::is_regular_file(renamed) && contents(renamed) == originalText);
        check("13_ORIGINAL_REMAINS", fs::is_regular_file(mainFile) && contents(mainFile) == originalText);
        run("14_RENAME_TO_EXISTING_REJECTED",
            renameFile(rootRel + "/Main.java", "Main_renamed.java"), ExecutionStatus::FAILED);

        r = run("15_CREATE_STAGING_FOLDER", folder(stagingName), ExecutionStatus::SUCCESS);
        stagingCreated = r.status == ExecutionStatus::SUCCESS && fs::is_directory(staging);
        check("16_STAGING_EXISTS", stagingCreated);
        if (!stagingCreated) throw runtime_error("staging creation failed");
        const string archiveRel = rootRel + "/archive";
        run("17_MOVE_STAGING_INTO_WORKSPACE", moveFile(stagingRel, archiveRel), ExecutionStatus::SUCCESS);
        stagingCreated = fs::exists(staging);
        const fs::path archive = root / "archive";
        check("18_ARCHIVE_EXISTS", fs::is_directory(archive));

        run("19_MOVE_RENAMED_FILE_TO_ARCHIVE",
            moveFile(rootRel + "/Main_renamed.java", archiveRel + "/Main_renamed.java"), ExecutionStatus::SUCCESS);
        const fs::path moved = archive / "Main_renamed.java";
        check("20_MOVED_FILE_CONTENT", fs::is_regular_file(moved) && contents(moved) == originalText);
        check("21_OLD_PATH_ABSENT", !fs::exists(renamed));
        run("22_MOVE_MISSING_SOURCE_REJECTED",
            moveFile(rootRel + "/absent.java", archiveRel + "/absent.java"), ExecutionStatus::FAILED);
        run("23_MOVE_EXISTING_DESTINATION_REJECTED",
            moveFile(rootRel + "/Main.java", archiveRel + "/Main_renamed.java"), ExecutionStatus::FAILED);

        run("24_DELETE_MOVED_FILE", deleteResource(archiveRel + "/Main_renamed.java"), ExecutionStatus::SUCCESS);
        check("25_DELETED_FILE_ABSENT", !fs::exists(moved));
        run("26_NONEMPTY_DIRECTORY_DELETE_REJECTED", deleteResource(rootRel), ExecutionStatus::FAILED);
        check("27_MAIN_FILE_PRESERVED", fs::is_regular_file(mainFile) && contents(mainFile) == originalText);
        run("28_DELETE_EMPTY_ARCHIVE", deleteResource(archiveRel), ExecutionStatus::SUCCESS);
        check("29_ARCHIVE_REMOVED", !fs::exists(archive));

        run("30_DELETE_TRAVERSAL_REJECTED",
            deleteResource("desktop/../NovaGroup2Escape"), ExecutionStatus::NOT_EXECUTED);
        run("31_COPY_TRAVERSAL_REJECTED",
            copyFile(rootRel + "/Main.java", "desktop/../NovaGroup2Escape.java"), ExecutionStatus::NOT_EXECUTED);
        run("32_MOVE_TRAVERSAL_REJECTED",
            moveFile(rootRel + "/Main.java", "desktop/../NovaGroup2Escape.java"), ExecutionStatus::NOT_EXECUTED);
        check("33_NO_ESCAPE_FILE_CREATED", !fs::exists(desk / "NovaGroup2Escape.java"));
    } catch (const exception& e) {
        cerr << "TEST ABORTED: " << e.what() << '\n';
        ++failed;
    }

    error_code rootEc, stagingEc;
    if (rootCreated && fs::exists(root)) fs::remove_all(root, rootEc);
    if (stagingCreated && fs::exists(staging)) fs::remove_all(staging, stagingEc);
    check("34_TEST_RESOURCES_CLEANED", !fs::exists(root) && !rootEc && !fs::exists(staging) && !stagingEc);

    cout << "\nGROUP 2 FILESYSTEM MANIPULATION TEST\nPassed: " << passed
         << "\nFailed: " << failed << '\n' << (failed == 0 ? "RESULT: PASS\n" : "RESULT: FAIL\n");
    return failed == 0 ? 0 : 1;
}
