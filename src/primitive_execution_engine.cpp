#include <filesystem>
#include <windows.h>
#include <shlobj.h>
#include <fstream>
#include <iostream>

#include "primitive_execution_engine.hpp"

using namespace std;

namespace
{
    filesystem::path getDesktopPath()
    {
        PWSTR desktopPath = nullptr;

        HRESULT result =
            SHGetKnownFolderPath(
                FOLDERID_Desktop,
                0,
                nullptr,
                &desktopPath
            );

        if (FAILED(result))
        {
            return {};
        }

        filesystem::path path(desktopPath);

        CoTaskMemFree(desktopPath);

        return path;
    }
}

ExecutionResult PrimitiveExecutionEngine::execute(
    const PrimitiveOperation& operation
)
{

    cout << "[DEBUG] type="
        << static_cast<int>(operation.type)
        << " name=[" << operation.name << "]"
        << endl;
    ExecutionResult result{};

    result.action = operation.name;
    result.confidence = 0.0;
    result.targetResolved = false;
    result.executionMechanismResolved = false;

    // --------------------------------------------------
    // Step 1: Resolve primitive
    // --------------------------------------------------

    if (!operation.resolved)
    {
        result.status =
            ExecutionStatus::NOT_EXECUTED;

        result.reason =
            ExecutionFailureReason::INVALID_PLAN;

        result.message =
            "Primitive operation is unresolved.";

        return result;
    }

        // --------------------------------------------------
    // Step 2: filesystem.create
    // --------------------------------------------------

    if (
        operation.type ==
            PrimitiveOperationType::FILESYSTEM_CREATE
        &&
        operation.name ==
            "filesystem.create"
    )
    {
        const string& name =
            operation.parameters.name;

        const string& location =
            operation.parameters.location;

        if (
            name.empty()
            ||
            location.empty()
        )
        {
            result.status =
                ExecutionStatus::NOT_EXECUTED;

            result.reason =
                ExecutionFailureReason::TARGET_NOT_FOUND;

            result.message =
                "Folder name or location is missing.";

            return result;
        }

        if (location != "desktop")
        {
            result.status =
                ExecutionStatus::NOT_EXECUTED;

            result.reason =
                ExecutionFailureReason::UNSUPPORTED_ACTION;

            result.message =
                "Unsupported folder location.";

            return result;
        }

        filesystem::path desktopPath =
            getDesktopPath();

        if (desktopPath.empty())
        {
            result.status =
                ExecutionStatus::FAILED;

            result.reason =
                ExecutionFailureReason::TARGET_NOT_FOUND;

            result.message =
                "Unable to resolve Windows Desktop location.";

            return result;
        }

        filesystem::path folderPath =
            desktopPath / name;

        result.targetResolved = true;
        result.executionMechanismResolved = true;
        result.confidence = 1.0;

        try
        {
            if (filesystem::exists(folderPath))
            {
                result.status =
                    ExecutionStatus::FAILED;

                result.reason =
                    ExecutionFailureReason::EXECUTION_ERROR;

                result.message =
                    "Folder already exists: "
                    + folderPath.string();

                return result;
            }

            if (
                filesystem::create_directories(
                    folderPath
                )
            )
            {
                result.status =
                    ExecutionStatus::SUCCESS;

                result.reason =
                    ExecutionFailureReason::NONE;

                result.message =
                    "Folder created: "
                    + folderPath.string();

                return result;
            }
        }
        catch (
            const filesystem::filesystem_error& error
        )
        {
            result.status =
                ExecutionStatus::FAILED;

            result.reason =
                ExecutionFailureReason::EXECUTION_ERROR;

            result.message =
                error.what();

            return result;
        }

        result.status =
            ExecutionStatus::FAILED;

        result.reason =
            ExecutionFailureReason::EXECUTION_ERROR;

        result.message =
            "Folder could not be created.";

        return result;
    }

    // --------------------------------------------------
    // Step 3: filesystem.create_file
    // --------------------------------------------------

    if (
        operation.type ==
            PrimitiveOperationType::FILESYSTEM_CREATE_FILE
        &&
        operation.name ==
            "filesystem.create_file"
    )
    {
        const string& name =
            operation.parameters.name;

        const string& location =
            operation.parameters.location;

        const string& content =
            operation.parameters.content;

        if (
            name.empty()
            ||
            location.empty()
        )
        {
            result.status =
                ExecutionStatus::NOT_EXECUTED;

            result.reason =
                ExecutionFailureReason::TARGET_NOT_FOUND;

            result.message =
                "File name or location is missing.";

            return result;
        }

        filesystem::path fileDirectory;

        if (location == "desktop")
        {
            fileDirectory =
                getDesktopPath();
        }
        else if (
            location.rfind("desktop/", 0) == 0
        )
        {
            filesystem::path relativePath =
                location.substr(
                    string("desktop/").length()
                );

            fileDirectory =
                getDesktopPath() / relativePath;
        }
        else
        {
            result.status =
                ExecutionStatus::NOT_EXECUTED;

            result.reason =
                ExecutionFailureReason::UNSUPPORTED_ACTION;

            result.message =
                "Unsupported file location.";

            return result;
        }

        if (fileDirectory.empty())
        {
            result.status =
                ExecutionStatus::FAILED;

            result.reason =
                ExecutionFailureReason::TARGET_NOT_FOUND;

            result.message =
                "Unable to resolve file location.";

            return result;
        }

        filesystem::path filePath =
            fileDirectory / name;

        result.targetResolved = true;
        result.executionMechanismResolved = true;
        result.confidence = 1.0;

        try
        {
            if (!filesystem::exists(fileDirectory))
            {
                result.status =
                    ExecutionStatus::FAILED;

                result.reason =
                    ExecutionFailureReason::TARGET_NOT_FOUND;

                result.message =
                    "Parent directory does not exist: "
                    + fileDirectory.string();

                return result;
            }

            if (filesystem::exists(filePath))
            {
                result.status =
                    ExecutionStatus::FAILED;

                result.reason =
                    ExecutionFailureReason::EXECUTION_ERROR;

                result.message =
                    "File already exists: "
                    + filePath.string();

                return result;
            }

            ofstream file(filePath);

            if (!file.is_open())
            {
                result.status =
                    ExecutionStatus::FAILED;

                result.reason =
                    ExecutionFailureReason::EXECUTION_ERROR;

                result.message =
                    "Unable to create file: "
                    + filePath.string();

                return result;
            }

            file << content;
            file.close();

            result.status =
                ExecutionStatus::SUCCESS;

            result.reason =
                ExecutionFailureReason::NONE;

            result.message =
                "File created: "
                + filePath.string();

            return result;
        }
        catch (
            const filesystem::filesystem_error& error
        )
        {
            result.status =
                ExecutionStatus::FAILED;

            result.reason =
                ExecutionFailureReason::EXECUTION_ERROR;

            result.message =
                error.what();

            return result;
        }
    }
    // --------------------------------------------------
    // Unsupported primitive
    // --------------------------------------------------

    result.status =
        ExecutionStatus::NOT_EXECUTED;

    result.reason =
        ExecutionFailureReason::UNSUPPORTED_ACTION;

    result.message =
        "No execution mechanism exists for primitive: "
        + operation.name;

    return result;
}