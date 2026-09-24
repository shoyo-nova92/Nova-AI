#include "execution_engine.hpp"

#include <iostream>

using namespace std;

void printResult(
    const string& testName,
    const ExecutionResult& result
)
{
    cout
        << "\n[" << testName << "]"
        << endl;

    cout
        << "Status: "
        << static_cast<int>(result.status)
        << endl;

    cout
        << "Reason: "
        << static_cast<int>(result.reason)
        << endl;

    cout
        << "Action: "
        << result.action
        << endl;

    cout
        << "Confidence: "
        << result.confidence
        << endl;

    cout
        << "Message: "
        << result.message
        << endl;
}

int main()
{
    ExecutionEngine engine;

    // --------------------------------------------------
    // Test 1: Successful folder creation
    // --------------------------------------------------

    ExecutionPlan successPlan{};

    successPlan.valid = true;
    successPlan.type = "single_action";
    successPlan.action.name = "create_folder";

    successPlan.capability.supported = true;
    successPlan.capability.name = "filesystem";

    successPlan.parameters.name = "NovaExecutionTest";
    successPlan.parameters.location = "desktop";

    ExecutionResult successResult =
        engine.execute(successPlan);

    printResult(
        "SUCCESS",
        successResult
    );

    // --------------------------------------------------
    // Test 2: Folder already exists
    // --------------------------------------------------

    ExecutionResult existingResult =
        engine.execute(successPlan);

    printResult(
        "FOLDER_ALREADY_EXISTS",
        existingResult
    );

    // --------------------------------------------------
    // Test 3: Missing folder name
    // --------------------------------------------------

    ExecutionPlan missingNamePlan{};

    missingNamePlan.valid = true;
    missingNamePlan.type = "single_action";
    missingNamePlan.action.name = "create_folder";

    missingNamePlan.capability.supported = true;
    missingNamePlan.capability.name = "filesystem";

    missingNamePlan.parameters.name = "";
    missingNamePlan.parameters.location = "desktop";

    ExecutionResult missingNameResult =
        engine.execute(missingNamePlan);

    printResult(
        "MISSING_FOLDER_NAME",
        missingNameResult
    );

    // --------------------------------------------------
    // Test 4: Unsupported location
    // --------------------------------------------------

    ExecutionPlan unsupportedLocationPlan{};

    unsupportedLocationPlan.valid = true;
    unsupportedLocationPlan.type = "single_action";
    unsupportedLocationPlan.action.name = "create_folder";

    unsupportedLocationPlan.capability.supported = true;
    unsupportedLocationPlan.capability.name = "filesystem";

    unsupportedLocationPlan.parameters.name =
        "NovaUnsupportedLocationTest";

    unsupportedLocationPlan.parameters.location =
        "somewhere";

    ExecutionResult unsupportedLocationResult =
        engine.execute(
            unsupportedLocationPlan
        );

    printResult(
        "UNSUPPORTED_LOCATION",
        unsupportedLocationResult
    );

    // --------------------------------------------------
    // Test 5: Unsupported action
    // --------------------------------------------------

    ExecutionPlan unsupportedActionPlan{};

    unsupportedActionPlan.valid = true;
    unsupportedActionPlan.type = "single_action";
    unsupportedActionPlan.action.name = "delete_everything";

    unsupportedActionPlan.capability.supported = true;
    unsupportedActionPlan.capability.name = "filesystem";

    unsupportedActionPlan.parameters.name =
        "test";

    unsupportedActionPlan.parameters.location =
        "desktop";

    ExecutionResult unsupportedActionResult =
        engine.execute(
            unsupportedActionPlan
        );

    printResult(
        "UNSUPPORTED_ACTION",
        unsupportedActionResult
    );

    // --------------------------------------------------
    // Test 6: Invalid execution plan
    // --------------------------------------------------

    ExecutionPlan invalidPlan{};

    invalidPlan.valid = false;
    invalidPlan.type = "single_action";
    invalidPlan.action.name = "create_folder";

    ExecutionResult invalidResult =
        engine.execute(invalidPlan);

    printResult(
        "INVALID_PLAN",
        invalidResult
    );

    ExecutionPlan openAppPlan;

    openAppPlan.valid = true;
    openAppPlan.type = "single_action";

    openAppPlan.action.name =
        "open_app";

    openAppPlan.capability.supported =
        true;

    openAppPlan.capability.name =
        "application_control";

    openAppPlan.parameters.application =
        "chrome";

    openAppPlan.message =
        "Valid execution plan.";

    ExecutionResult openAppResult =
        engine.execute(
            openAppPlan
        );

    printResult(
        "OPEN_APP",
        openAppResult
    );

    return 0;
}