#include <filesystem>
#include <cstdlib>
#include <windows.h>
#include <shlobj.h>
#include <shellapi.h>
#include "execution_engine.hpp"
#include "application_resolver.hpp"

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
    bool openApplication(
        const ExecutionPlan& plan,
        std::string& message
    )
    {
        if (plan.parameters.application.empty())
        {
            message =
                "Application name is missing.";

            return false;
        }

        ApplicationResolver resolver;

        ApplicationResolverResult resolved =
            resolver.resolve(
                plan.parameters.application
            );

        if (!resolved.success)
        {
            message = resolved.message;
            return false;
        }

        const std::string shellTarget =
            "shell:AppsFolder\\" +
            resolved.appId;

        HINSTANCE result =
            ShellExecuteA(
                nullptr,
                "open",
                shellTarget.c_str(),
                nullptr,
                nullptr,
                SW_SHOWNORMAL
            );

        if (
            reinterpret_cast<INT_PTR>(result)
            <= 32
        )
        {
            message =
                "Windows failed to launch application: "
                + resolved.displayName;

            return false;
        }

        message =
            "Application launched: "
            + resolved.displayName;

        return true;
    }
    bool createFolder(
    const ExecutionPlan& plan,
    string& message
)
{
    if (
        plan.parameters.name.empty()
        ||
        plan.parameters.location.empty()
    )
    {
        message =
            "Folder name or location is missing.";

        return false;
    }

    filesystem::path basePath;

    if (plan.parameters.location == "desktop")
    {
        basePath =
            getDesktopPath();

        if (basePath.empty())
        {
            message =
                "Unable to resolve Windows Desktop location.";

            return false;
        }
    }
    else
    {
        message =
            "Unsupported folder location.";

        return false;
    }

    filesystem::path folderPath =
        basePath
        /
        plan.parameters.name;

    try
    {
        if (filesystem::exists(folderPath))
        {
            message =
                "Folder already exists: "
                + folderPath.string();

            return false;
        }

        if (
            filesystem::create_directories(
                folderPath
            )
        )
        {
            message =
                "Folder created: "
                + folderPath.string();

            return true;
        }
    }
    catch (const filesystem::filesystem_error& error)
    {
        message =
            error.what();

        return false;
    }

    message =
        "Folder could not be created.";

    return false;
}
    constexpr double EXECUTION_CONFIDENCE_THRESHOLD = 0.80;
}
ExecutionResult ExecutionEngine::execute(
    const ExecutionPlan& plan
)
{
    ExecutionResult result{};

    result.action =
        plan.action.name;

    result.confidence =
        0.0;

    result.targetResolved =
        false;

    result.executionMechanismResolved =
        false;

    // --------------------------------------------------
    // Step 1: Validate execution plan
    // --------------------------------------------------

    if (!plan.valid)
    {
        result.status =
            ExecutionStatus::NOT_EXECUTED;

        result.reason =
            ExecutionFailureReason::INVALID_PLAN;

        result.message =
            "Execution plan is invalid.";

        return result;
    }

    // --------------------------------------------------
    // Step 2: Base execution evidence
    // --------------------------------------------------

    if (!plan.action.name.empty())
    {
        result.confidence += 0.20;
    }

    if (plan.capability.supported)
    {
        result.confidence += 0.20;
    }

    if (!plan.capability.name.empty())
    {
        result.confidence += 0.10;
    }

    if (
        !plan.parameters.name.empty()
        ||
        !plan.parameters.location.empty()
        ||
        !plan.parameters.application.empty()
    )
    {
        result.confidence += 0.10;
    }

    // --------------------------------------------------
    // Step 3: Resolve execution target and mechanism
    // --------------------------------------------------

    if (plan.action.name == "open_app")
    {
        if (plan.parameters.application.empty())
        {
            result.status =
                ExecutionStatus::NOT_EXECUTED;

            result.reason =
                ExecutionFailureReason::TARGET_NOT_FOUND;

            result.message =
                "Application target is missing.";

            return result;
        }

        ApplicationResolver resolver;

        ApplicationResolverResult resolved =
            resolver.resolve(
                plan.parameters.application
            );

        if (!resolved.success)
        {
            result.status =
                ExecutionStatus::NOT_EXECUTED;

            result.reason =
                ExecutionFailureReason::TARGET_NOT_FOUND;

            result.message =
                resolved.message;

            return result;
        }

        result.targetResolved =
            true;

        const std::string shellTarget =
            "shell:AppsFolder\\"
            +
            resolved.appId;

        result.executionMechanismResolved =
            true;

        result.confidence += 0.20;
        result.confidence += 0.20;

        if (
            result.confidence
            <
            EXECUTION_CONFIDENCE_THRESHOLD
        )
        {
            result.status =
                ExecutionStatus::NOT_EXECUTED;

            result.reason =
                ExecutionFailureReason::LOW_CONFIDENCE;

            result.message =
                "Execution confidence is below the required threshold.";

            return result;
        }

        // --------------------------------------------------
        // Step 4: Execute application
        // --------------------------------------------------

        HINSTANCE launchResult =
            ShellExecuteA(
                nullptr,
                "open",
                shellTarget.c_str(),
                nullptr,
                nullptr,
                SW_SHOWNORMAL
            );

        if (
            reinterpret_cast<INT_PTR>(launchResult)
            <= 32
        )
        {
            result.status =
                ExecutionStatus::FAILED;

            result.reason =
                ExecutionFailureReason::EXECUTION_ERROR;

            result.message =
                "Windows failed to launch application: "
                +
                resolved.displayName;

            return result;
        }

        result.status =
            ExecutionStatus::SUCCESS;

        result.reason =
            ExecutionFailureReason::NONE;

        result.message =
            "Application launched: "
            +
            resolved.displayName;

        return result;
    }

    // --------------------------------------------------
    // Step 5: Create folder
    // --------------------------------------------------

    if (plan.action.name == "create_folder")
    {
        result.targetResolved =
    !plan.parameters.name.empty()
    &&
    plan.parameters.location == "desktop";

result.executionMechanismResolved =
    result.targetResolved;
        if (result.targetResolved)
        {
            result.confidence += 0.20;
        }

        if (result.executionMechanismResolved)
        {
            result.confidence += 0.20;
        }

        if (
            result.confidence
            <
            EXECUTION_CONFIDENCE_THRESHOLD
        )
        {
            result.status =
                ExecutionStatus::NOT_EXECUTED;

            result.reason =
                ExecutionFailureReason::LOW_CONFIDENCE;

            result.message =
                "Execution confidence is below the required threshold.";

            return result;
        }

        std::string message;

        if (createFolder(plan, message))
        {
            result.status =
                ExecutionStatus::SUCCESS;

            result.reason =
                ExecutionFailureReason::NONE;

            result.message =
                message;

            return result;
        }

        result.status =
            ExecutionStatus::FAILED;

        result.reason =
            ExecutionFailureReason::EXECUTION_ERROR;

        result.message =
            message;

        return result;
    }

    // --------------------------------------------------
    // Step 6: Unsupported action
    // --------------------------------------------------

    result.status =
        ExecutionStatus::NOT_EXECUTED;

    result.reason =
        ExecutionFailureReason::UNSUPPORTED_ACTION;

    result.message =
        "No execution mechanism exists for action: "
        +
        plan.action.name;

    return result;
}