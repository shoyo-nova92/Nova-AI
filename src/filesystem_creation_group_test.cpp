// Windows headers must precede project headers that expose `using namespace std;`.
#include <windows.h>
#include <shlobj.h>

#include "primitive_execution_engine.hpp"
#include "utf8_text.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

using namespace std;
namespace fs = std::filesystem;

namespace {
fs::path offlineParent;
int passed = 0;
int failed = 0;

void check(const string& name, bool condition) {
    cout << "[" << name << "] " << (condition ? "PASS" : "FAIL") << '\n';
    if (condition) ++passed;
    else ++failed;
}

fs::path desktopPath() {
    if(!offlineParent.empty())return offlineParent;
    PWSTR rawPath = nullptr;
    HRESULT hr = SHGetKnownFolderPath(FOLDERID_Desktop, 0, nullptr, &rawPath);
    if (FAILED(hr) || rawPath == nullptr) return {};
    fs::path result(rawPath);
    CoTaskMemFree(rawPath);
    return result;
}

PrimitiveOperation createFolder(const string& name) {
    PrimitiveOperation op{};
    op.type = PrimitiveOperationType::FILESYSTEM_CREATE;
    op.name = "filesystem.create";
    op.parameters.name = name;
    op.parameters.location = offlineParent.empty()?"desktop":Utf8::path(offlineParent);
    op.resolved = true;
    return op;
}

PrimitiveOperation createFile(
    const string& name, const string& location, const string& content = "") {
    PrimitiveOperation op{};
    op.type = PrimitiveOperationType::FILESYSTEM_CREATE_FILE;
    op.name = "filesystem.create_file";
    op.parameters.name = name;
    op.parameters.location = location;
    op.parameters.content = content;
    op.resolved = true;
    return op;
}

string readAll(const fs::path& path) {
    ifstream file(path, ios::in | ios::binary);
    return string(istreambuf_iterator<char>(file), istreambuf_iterator<char>());
}
}

int main(int argc,char** argv) {
    if(argc==2 && string(argv[1])=="--offline")offlineParent=fs::temp_directory_path();
    else if(argc!=1)return 2;
    cout << "=== GROUP 1: FILESYSTEM CREATION TEST ===\n";
    cout << (offlineParent.empty()?"Tests use a unique directory on Desktop.\n":"Offline tests use a unique temporary directory.\n");

    const fs::path desktop = desktopPath();
    if (desktop.empty() || !fs::exists(desktop)) {
        cerr << "Cannot resolve the current Windows Desktop. Aborting.\n";
        return 2;
    }

    const auto stamp = chrono::steady_clock::now().time_since_epoch().count();
    const string rootName = "NovaGroup1Test_" + to_string(stamp);
    const fs::path rootPath = desktop / rootName;
    const string rootLocation = offlineParent.empty()?"desktop/"+rootName:Utf8::path(rootPath);
    PrimitiveExecutionEngine engine;

    // Safety guard: never clean a path unless it is the unique test directory we selected.
    if (fs::exists(rootPath)) {
        cerr << "Unique test path unexpectedly exists; refusing cleanup.\n";
        return 2;
    }

    try {
        // 1. Create folder and verify real filesystem state.
        auto folderResult = engine.execute(createFolder(rootName));
        check("CREATE_FOLDER_SUCCESS", folderResult.status == ExecutionStatus::SUCCESS);
        check("CREATE_FOLDER_EXISTS_ON_DISK",
              folderResult.status == ExecutionStatus::SUCCESS &&
              fs::is_directory(rootPath));

        // 2. Duplicate folder must fail and leave existing folder intact.
        auto duplicateFolder = engine.execute(createFolder(rootName));
        check("EXISTING_FOLDER_REJECTED",
              duplicateFolder.status == ExecutionStatus::FAILED);
        check("EXISTING_FOLDER_PRESERVED", fs::is_directory(rootPath));

        // 3. Create file and verify exact bytes.
        const string fileContent = "public class Main {\n    public static void main(String[] args) {}\n}\n";
        auto fileResult = engine.execute(createFile("Main.java", rootLocation, fileContent));
        const fs::path filePath = rootPath / "Main.java";
        check("CREATE_FILE_SUCCESS", fileResult.status == ExecutionStatus::SUCCESS);
        check("FILE_EXISTS_ON_DISK", fs::is_regular_file(filePath));
        check("FILE_CONTENT_EXACT", fs::is_regular_file(filePath) && readAll(filePath) == fileContent);

        // 4. Existing file must not be truncated or overwritten.
        auto duplicateFile = engine.execute(createFile("Main.java", rootLocation, "overwrite attempt"));
        check("EXISTING_FILE_REJECTED", duplicateFile.status == ExecutionStatus::FAILED);
        check("EXISTING_FILE_CONTENT_PRESERVED",
              fs::is_regular_file(filePath) && readAll(filePath) == fileContent);

        // The manual alias test exercises missing-location discovery. Offline
        // coverage uses an invalid file-as-parent, avoiding discovery outside
        // the test fixture while checking rejection and no implicit creation.
        const string invalidParent = rootLocation + (offlineParent.empty()?"/does-not-exist":"/Main.java");
        auto missingParent = engine.execute(createFile("Missing.java", invalidParent, "class Missing {}"));
        check("MISSING_OR_INVALID_PARENT_REJECTED", missingParent.status == ExecutionStatus::FAILED || missingParent.status == ExecutionStatus::NOT_EXECUTED);
        check("MISSING_PARENT_NOT_CREATED", !fs::exists(rootPath / "does-not-exist"));

        // 6. Unsafe leaf names must be rejected.
        auto unsafeFolder = engine.execute(createFolder("../NovaGroup1Escape"));
        check("FOLDER_PATH_TRAVERSAL_REJECTED",
              unsafeFolder.status == ExecutionStatus::NOT_EXECUTED);
        auto unsafeFileName = engine.execute(createFile("../NovaGroup1Escape.txt", rootLocation, "x"));
        check("FILE_PATH_TRAVERSAL_REJECTED",
              unsafeFileName.status == ExecutionStatus::NOT_EXECUTED);
        check("NO_ESCAPE_FILE_CREATED", !fs::exists(desktop / "NovaGroup1Escape.txt"));

        // 7. Unsafe location traversal must be rejected.
        auto unsafeLocation = engine.execute(
            createFile("NovaGroup1Escape.txt", "desktop/../NovaGroup1Escape", "x"));
        check("LOCATION_PATH_TRAVERSAL_REJECTED",
              unsafeLocation.status == ExecutionStatus::NOT_EXECUTED);
        check("NO_ESCAPE_DIRECTORY_CREATED", !fs::exists(desktop / "NovaGroup1Escape"));

        // 8. Unresolved operations must not execute.
        auto unresolved = createFolder(rootName + "_unresolved");
        unresolved.resolved = false;
        auto unresolvedResult = engine.execute(unresolved);
        check("UNRESOLVED_OPERATION_REJECTED",
              unresolvedResult.status == ExecutionStatus::NOT_EXECUTED);
        check("UNRESOLVED_FOLDER_NOT_CREATED",
              !fs::exists(desktop / (rootName + "_unresolved")));
    }
    catch (const exception& error) {
        cerr << "Unexpected test exception: " << error.what() << '\n';
        ++failed;
    }

    // Remove only the unique test directory created by this test.
    error_code cleanupError;
    if (fs::exists(rootPath))
        fs::remove_all(rootPath, cleanupError);
    check("TEST_DIRECTORY_CLEANED", !fs::exists(rootPath) && !cleanupError);

    cout << "\nGROUP 1 FILESYSTEM CREATION TEST\n";
    cout << "Passed: " << passed << "\nFailed: " << failed << '\n';
    cout << (failed == 0 ? "RESULT: PASS\n" : "RESULT: FAIL\n");
    return failed == 0 ? 0 : 1;
}
