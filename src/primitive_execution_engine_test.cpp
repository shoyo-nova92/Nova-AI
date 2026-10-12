#include <iostream>
#include <filesystem>

#include "primitive_execution_engine.hpp"

using namespace std;

int main()
{
    cout << "=== PrimitiveExecutionEngine Test ===" << endl;

    PrimitiveExecutionEngine engine;
    const filesystem::path testFolder =
    filesystem::path(
        "C:/Users/shour/OneDrive/Desktop/NovaPrimitiveTest"
    );

filesystem::remove_all(testFolder);

    const string folderName = "NovaPrimitiveTest";
    const string fileName = "Main.java";

    // --------------------------------------------------
    // STEP 1: Create folder
    // --------------------------------------------------

    PrimitiveOperation createFolder{};
    createFolder.type =
        PrimitiveOperationType::FILESYSTEM_CREATE;

    createFolder.name =
        "filesystem.create";

    createFolder.parameters.name =
        folderName;

    createFolder.parameters.location =
        "desktop";

    createFolder.resolved = true;

    ExecutionResult folderResult =
        engine.execute(createFolder);

    cout << "\n[STEP 1]" << endl;
    cout << "[MESSAGE] "
         << folderResult.message
         << endl;

    const bool folderSuccess =
        folderResult.status == ExecutionStatus::SUCCESS;

    cout << "[ASSERT] filesystem.create : "
         << (folderSuccess ? "PASS" : "FAIL")
         << endl;

    if (!folderSuccess)
        return 1;


    // --------------------------------------------------
    // STEP 2: Create file inside folder
    // --------------------------------------------------

    PrimitiveOperation createFile{};
    createFile.type =
        PrimitiveOperationType::FILESYSTEM_CREATE_FILE;

    createFile.name =
        "filesystem.create_file";

    createFile.parameters.name =
        fileName;

    createFile.parameters.location =
        "desktop/" + folderName;

    createFile.parameters.content =
        "public class Main {}";

    createFile.resolved = true;

    ExecutionResult fileResult =
        engine.execute(createFile);

    cout << "\n[STEP 2]" << endl;
    cout << "[MESSAGE] "
         << fileResult.message
         << endl;

    const bool fileSuccess =
        fileResult.status == ExecutionStatus::SUCCESS;

    cout << "[ASSERT] filesystem.create_file : "
         << (fileSuccess ? "PASS" : "FAIL")
         << endl;


    // --------------------------------------------------
    // Final result
    // --------------------------------------------------

    const bool success =
        folderSuccess && fileSuccess;

    cout << "\n=== RESULT ===" << endl;
    cout << (success ? "PASS" : "FAIL")
         << endl;

    return success ? 0 : 1;
}