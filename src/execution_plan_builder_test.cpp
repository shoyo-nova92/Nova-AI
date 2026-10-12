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
    parameters.name = ParameterValue{};
    parameters.location = ParameterValue{};
    parameters.application = application.empty()
        ? ParameterValue{}
        : ParameterValue{ParameterValueType::LITERAL, application, {}};

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

void runMultiStepTest(
    ExecutionPlanBuilder& builder
)
{
    MultiStepPlan plan;

    plan.valid = true;
    plan.goal =
        "Create NovaJava and open it in VS Code.";

    MultiStepAction step1;
    step1.id = "step_1";
    step1.description =
        "Create NovaJava directory.";
    step1.dependencies = {};
    step1.resolved = true;

    PrimitiveOperation operation1;
    operation1.type =
        PrimitiveOperationType::FILESYSTEM_CREATE;
    operation1.name =
        "filesystem.create";
    operation1.parameters.name =
        "NovaJava";
    operation1.parameters.location =
        "desktop";
    operation1.resolved = true;

    step1.operations.push_back(operation1);


    MultiStepAction step2;
    step2.id = "step_2";
    step2.description =
        "Create Main.java inside NovaJava.";
    step2.dependencies = {"step_1"};
    step2.resolved = true;

    PrimitiveOperation operation2;
    operation2.type =
        PrimitiveOperationType::FILESYSTEM_CREATE_FILE;
    operation2.name =
        "filesystem.create_file";
    operation2.parameters.name =
        "Main.java";
    operation2.parameters.location =
        "desktop/NovaJava";
    operation2.parameters.content =
        "public class Main {}";
    operation2.resolved = true;

    step2.operations.push_back(operation2);


    MultiStepAction step3;
    step3.id = "step_3";
    step3.description =
        "Open NovaJava in VS Code.";
    step3.dependencies = {"step_2"};
    step3.resolved = true;

    PrimitiveOperation operation3;
    operation3.type =
        PrimitiveOperationType::APPLICATION_LAUNCH;
    operation3.name =
        "application.launch";
    operation3.parameters.application =
        "VS Code";
    operation3.parameters.target =
        "desktop/NovaJava";
    operation3.resolved = true;

    step3.operations.push_back(operation3);


    plan.steps.push_back(step1);
    plan.steps.push_back(step2);
    plan.steps.push_back(step3);


    vector<string> executionOrder = {
        "step_1",
        "step_2",
        "step_3"
    };


    ExecutionPlan executionPlan =
        builder.build(
            plan,
            executionOrder
        );


    cout << endl;
    cout << "============================================================"
         << endl;

    cout << "B3 MULTI-STEP EXECUTION PLAN TEST"
         << endl;

    cout << "============================================================"
         << endl;


    cout << "Valid: "
         << (executionPlan.valid ? "true" : "false")
         << endl;

    cout << "Type: "
         << executionPlan.type
         << endl;

    cout << "Steps: "
         << executionPlan.steps.size()
         << endl;

    for (const ExecutionPlanStep& step :
         executionPlan.steps)
    {
        cout << endl;

        cout << "Step: "
             << step.id
             << endl;

        cout << "Primitive: "
             << step.operation.name
             << endl;

        cout << "Dependencies: ";

        if (step.dependencies.empty())
        {
            cout << "[none]";
        }
        else
        {
            for (const string& dependency :
                 step.dependencies)
            {
                cout << dependency << " ";
            }
        }

        cout << endl;

        cout << "Name: "
             << (
                    step.operation.parameters.name.empty()
                    ? "[none]"
                    : step.operation.parameters.name
                )
             << endl;

        cout << "Location: "
             << (
                    step.operation.parameters.location.empty()
                    ? "[none]"
                    : step.operation.parameters.location
                )
             << endl;

        cout << "Application: "
             << (
                    step.operation.parameters.application.empty()
                    ? "[none]"
                    : step.operation.parameters.application
                )
             << endl;

        cout << "Target: "
             << (
                    step.operation.parameters.target.empty()
                    ? "[none]"
                    : step.operation.parameters.target
                )
             << endl;

        cout << "Content: "
             << (
                    step.operation.parameters.content.empty()
                    ? "[none]"
                    : step.operation.parameters.content
                )
             << endl;

        cout << "Valid: "
             << (step.valid ? "true" : "false")
             << endl;
    }

    cout << endl;

    cout << "Message: "
         << executionPlan.message
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
    runMultiStepTest(builder);

    return 0;
}