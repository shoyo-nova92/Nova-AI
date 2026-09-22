#include "action_validator.hpp"

#include <iostream>

using namespace std;

void runTest(
    ActionValidator& validator,
    const string& actionName,
    bool supported,
    const string& capabilityName,
    const string& name,
    const string& location,
    const string& application
)
{
    Action action;
    action.name = actionName;

    Capability capability;
    capability.supported = supported;
    capability.name = capabilityName;

    ActionParameters parameters;
    parameters.name = name;
    parameters.location = location;
    parameters.application = application;

    ActionValidationResult result =
        validator.validate(
            action,
            capability,
            parameters
        );

    cout << endl;
    cout << "Action: "
         << action.name
         << endl;

    cout << "Capability: "
         << capability.name
         << endl;

    cout << "Valid: "
         << (result.valid ? "true" : "false")
         << endl;

    cout << "Message: "
         << result.message
         << endl;
}

int main()
{
    ActionValidator validator;

    cout << "============================================================"
         << endl;

    cout << "ACTION VALIDATOR TEST"
         << endl;

    cout << "============================================================"
         << endl;

    // TEST 1 — VALID CREATE FOLDER
    runTest(
        validator,
        "create_folder",
        true,
        "filesystem",
        "nova",
        "desktop",
        ""
    );

    // TEST 2 — MISSING FOLDER NAME
    runTest(
        validator,
        "create_folder",
        true,
        "filesystem",
        "",
        "desktop",
        ""
    );

    // TEST 3 — VALID OPEN APP
    runTest(
        validator,
        "open_app",
        true,
        "application_control",
        "",
        "",
        "chrome"
    );

    // TEST 4 — MISSING APPLICATION
    runTest(
        validator,
        "open_app",
        true,
        "application_control",
        "",
        "",
        ""
    );

    // TEST 5 — UNSUPPORTED CAPABILITY
    runTest(
        validator,
        "open_app",
        false,
        "",
        "",
        "",
        "chrome"
    );

    // TEST 6 — UNKNOWN ACTION
    runTest(
        validator,
        "unknown_action",
        true,
        "filesystem",
        "",
        "",
        ""
    );

    return 0;
}