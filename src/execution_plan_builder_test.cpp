#include "execution_plan_builder.hpp"

#include <iostream>

using namespace std;

void runTest(
    ExecutionPlanBuilder& builder,
    const string& actionName,
    const string& capabilityName,
    const string& application,
    bool validationValid
)
{
    Action action;
    action.name = actionName;

    Capability capability;
    capability.supported = true;
    capability.name = capabilityName;
    capability.message = "Test capability.";

    ActionParameters parameters;
    parameters.name = "";
    parameters.location = "";
    parameters.application = application;

    ActionValidationResult validation;
    validation.valid = validationValid;
    validation.message =
        validationValid
        ? "Validation passed."
        : "Validation failed.";

    ExecutionPlan plan =
        builder.build(
            action,
            capability,
            parameters,
            validation
        );

    cout << endl;

    cout << "Action: "
         << action.name
         << endl;

    cout << "Capability: "
         << capability.name
         << endl;

    cout << "Application: "
         << (
                plan.parameters.application.empty()
                ? "[none]"
                : plan.parameters.application
            )
         << endl;

    cout << "Valid: "
         << (plan.valid ? "true" : "false")
         << endl;

    cout << "Type: "
         << (
                plan.type.empty()
                ? "[none]"
                : plan.type
            )
         << endl;

    cout << "Message: "
         << plan.message
         << endl;
}

int main()
{
    ExecutionPlanBuilder builder;

    cout << "============================================================"
         << endl;

    cout << "EXECUTION PLAN BUILDER TEST"
         << endl;

    cout << "============================================================"
         << endl;

    // TEST 1 — VALID PLAN
    runTest(
        builder,
        "open_app",
        "application_control",
        "chrome",
        true
    );

    // TEST 2 — INVALID ACTION
    runTest(
        builder,
        "open_app",
        "application_control",
        "chrome",
        false
    );

    return 0;
}