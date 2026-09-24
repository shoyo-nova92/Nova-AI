#include "single_action_pipeline.hpp"

#include <iostream>

using namespace std;

void runTest(
    SingleActionPipeline& pipeline,
    const string& input
)
{
    SingleActionResult result =
        pipeline.process(input);

    cout << endl;
    cout << "Input: "
         << input
         << endl;

    cout << "Success: "
         << (result.success ? "true" : "false")
         << endl;

    cout << "Action: "
         << (
                result.action.name.empty()
                ? "[none]"
                : result.action.name
            )
         << endl;

    cout << "Capability: "
         << (
                result.capability.name.empty()
                ? "[none]"
                : result.capability.name
            )
         << endl;

    cout << "Name: "
         << (
                result.parameters.name.empty()
                ? "[none]"
                : result.parameters.name
            )
         << endl;

    cout << "Location: "
         << (
                result.parameters.location.empty()
                ? "[none]"
                : result.parameters.location
            )
         << endl;

    cout << "Application: "
         << (
                result.parameters.application.empty()
                ? "[none]"
                : result.parameters.application
            )
         << endl;

    cout << "Valid: "
        << (
                result.validation.valid
                ? "true"
                : "false"
            )
        << endl;

    cout << "Execution Plan Valid: "
        << (
                result.executionPlan.valid
                ? "true"
                : "false"
            )
        << endl;

    cout << "Execution Plan Type: "
        << (
                result.executionPlan.type.empty()
                ? "[none]"
                : result.executionPlan.type
            )
        << endl;

    cout << "Execution Plan Action: "
        << (
                result.executionPlan.action.name.empty()
                ? "[none]"
                : result.executionPlan.action.name
            )
        << endl;

    cout << "Execution Plan Capability: "
        << (
                result.executionPlan.capability.name.empty()
                ? "[none]"
                : result.executionPlan.capability.name
            )
        << endl;
        cout << "Execution Status: "
            << static_cast<int>(
                result.executionResult.status
            )
            << endl;

        cout << "Execution Reason: "
            << static_cast<int>(
                result.executionResult.reason
            )
            << endl;

        cout << "Execution Action: "
            << (
                result.executionResult.action.empty()
                ? "[none]"
                : result.executionResult.action
            )
            << endl;

        cout << "Execution Confidence: "
            << result.executionResult.confidence
            << endl;

        cout << "Execution Message: "
            << (
                result.executionResult.message.empty()
                ? "[none]"
                : result.executionResult.message
            )
            << endl;

    cout << "Message: "
        << result.message
        << endl;
}

int main()
{
    SingleActionPipeline pipeline;

    cout << "============================================================"
         << endl;

    cout << "SINGLE ACTION PIPELINE TEST"
         << endl;

    cout << "============================================================"
         << endl;

    // TEST 1 — CREATE FOLDER
    runTest(
        pipeline,
        "create a folder called nova on my desktop"
    );

    // TEST 2 — OPEN APPLICATION
    runTest(
        pipeline,
        "open chrome"
    );

    // TEST 3 — CLOSE APPLICATION
    runTest(
        pipeline,
        "close spotify"
    );

    // TEST 4 — INVALID APPLICATION PARAMETER
    runTest(
        pipeline,
        "open "
    );

    // TEST 5 — UNKNOWN ACTION
    runTest(
        pipeline,
        "do something completely unsupported"
    );
    return 0;
}