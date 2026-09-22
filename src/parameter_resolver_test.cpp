#include "parameter_resolver.hpp"

#include <iostream>

using namespace std;

void runTest(
    ParameterResolver& resolver,
    const string& input,
    const string& actionName,
    const string& capabilityName
)
{
    Action action;
    action.name = actionName;

    Capability capability;
    capability.supported = true;
    capability.name = capabilityName;
    capability.message = "Test capability.";

    ParameterResolverResult result =
        resolver.resolve(
            input,
            action,
            capability
        );

    cout << endl;
    cout << "Input: "
         << input
         << endl;

    cout << "Action: "
         << action.name
         << endl;

    cout << "Capability: "
         << capability.name
         << endl;

    cout << "Success: "
         << (result.success ? "true" : "false")
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

    cout << "Message: "
         << result.message
         << endl;
}

int main()
{
    ParameterResolver resolver;

    cout << "============================================================"
         << endl;

    cout << "PARAMETER RESOLVER TEST"
         << endl;

    cout << "============================================================"
         << endl;

    // TEST 1 — CREATE FOLDER
    runTest(
        resolver,
        "create a folder called nova on my desktop",
        "create_folder",
        "filesystem"
    );

    // TEST 2 — OPEN APPLICATION
    runTest(
        resolver,
        "open chrome",
        "open_app",
        "application_control"
    );

    // TEST 3 — LAUNCH APPLICATION
    runTest(
        resolver,
        "launch vscode",
        "open_app",
        "application_control"
    );

    // TEST 4 — CLOSE APPLICATION
    runTest(
        resolver,
        "close spotify",
        "close_app",
        "application_control"
    );

    // TEST 5 — MISSING APPLICATION
    runTest(
        resolver,
        "open ",
        "open_app",
        "application_control"
    );

    // TEST 6 — EMPTY INPUT
    runTest(
        resolver,
        "",
        "open_app",
        "application_control"
    );

    return 0;
}