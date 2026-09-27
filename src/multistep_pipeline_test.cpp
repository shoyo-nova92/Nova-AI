#include "multistep_pipeline.hpp"

#include <iostream>
using namespace std;

int main()
{
    MultiStepPipeline pipeline;

    const string goal =
        "Create a Java project named NovaJava on my desktop, add a basic Main.java class, and open the project in VS Code.";

    cout << "============================================================"
         << endl;

    cout << "NOVA B3 MULTI-STEP PIPELINE TEST"
         << endl;

    cout << "============================================================"
         << endl;

    cout << endl;

    cout << "[GOAL]"
         << endl;

    cout << goal
         << endl;

    cout << endl;

    cout << "[PIPELINE]"
         << endl;

    MultiStepPipelineResult result =
        pipeline.run(
            goal
        );

    cout << endl;

    cout << "============================================================"
         << endl;

    cout << "[RESULT]"
         << endl;

    cout << "============================================================"
         << endl;

    cout << "Status: ";

    switch (result.status)
    {
        case MultiStepPipelineStatus::SUCCESS:
            cout << "SUCCESS";
            break;

        case MultiStepPipelineStatus::CLARIFICATION_REQUIRED:
            cout << "CLARIFICATION_REQUIRED";
            break;

        case MultiStepPipelineStatus::FAILED:
            cout << "FAILED";
            break;
    }

    cout << endl;

    cout << "Message: "
         << result.message
         << endl;

    cout << endl;

    if (
        result.status !=
        MultiStepPipelineStatus::SUCCESS
    )
    {
        return 1;
    }

    cout << "[PLAN]"
         << endl;

    cout << "Goal: "
         << result.plan.goal
         << endl;

    cout << "Step Count: "
         << result.plan.steps.size()
         << endl;

    cout << endl;

    for (
        const MultiStepAction& step
        : result.plan.steps
    )
    {
        cout << "----------------------------------------"
             << endl;

        cout << "Step ID: "
             << step.id
             << endl;

        cout << "Description: "
             << step.description
             << endl;

        cout << "Dependencies: ";

        if (step.dependencies.empty())
        {
            cout << "[none]";
        }
        else
        {
            for (
                const string& dependency
                : step.dependencies
            )
            {
                cout << dependency << " ";
            }
        }

        cout << endl;

        cout << "Operations: "
             << step.operations.size()
             << endl;

        for (
            const PrimitiveOperation& operation
            : step.operations
        )
        {
            cout << "  Primitive: "
                 << operation.name
                 << endl;

            cout << "  Resolved: "
                 << (
                        operation.resolved
                        ? "true"
                        : "false"
                    )
                 << endl;

            cout << "  Name: "
                 << (
                        operation.parameters.name.empty()
                        ? "[none]"
                        : operation.parameters.name
                    )
                 << endl;

            cout << "  Location: "
                 << (
                        operation.parameters.location.empty()
                        ? "[none]"
                        : operation.parameters.location
                    )
                 << endl;

            cout << "  Application: "
                 << (
                        operation.parameters.application.empty()
                        ? "[none]"
                        : operation.parameters.application
                    )
                 << endl;

            cout << "  Target: "
                 << (
                        operation.parameters.target.empty()
                        ? "[none]"
                        : operation.parameters.target
                    )
                 << endl;

            cout << "  Content: "
                 << (
                        operation.parameters.content.empty()
                        ? "[none]"
                        : operation.parameters.content
                    )
                 << endl;
        }

        cout << "Context References: "
             << step.contextReferences.size()
             << endl;

        for (
            const ContextReference& reference
            : step.contextReferences
        )
        {
            cout << "  Reference: "
                 << reference.reference
                 << endl;

            cout << "  Source Step: "
                 << reference.sourceStepId
                 << endl;

            cout << "  Resolved: "
                 << (
                        reference.resolved
                        ? "true"
                        : "false"
                    )
                 << endl;

            cout << "  Value: "
                 << (
                        reference.resolvedValue.empty()
                        ? "[none]"
                        : reference.resolvedValue
                    )
                 << endl;
        }

        cout << endl;
    }

    cout << "============================================================"
         << endl;

    cout << "REAL B3 PIPELINE EXECUTION COMPLETE"
         << endl;

    cout << "============================================================"
         << endl;

    return 0;
}