#include "multistep_pipeline.hpp"

#include <iostream>
using namespace std;


bool assertCondition(
    bool condition,
    const string& name
)
{
    cout << "[ASSERT] "
         << name
         << " : "
         << (condition ? "PASS" : "FAIL")
         << endl;

    return condition;
}

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

        cout << endl;

    cout << "[B3 INTEGRATION ASSERTIONS]"
         << endl;

    bool testsPassed = true;

    testsPassed &=
        assertCondition(
            result.executionPlan.valid,
            "Execution plan is valid"
        );

    testsPassed &=
        assertCondition(
            result.executionPlan.type == "multi_step",
            "Execution plan type is multi_step"
        );

    testsPassed &=
        assertCondition(
            result.executionPlan.steps.size() == 3,
            "Execution plan contains 3 steps"
        );

    if (result.executionPlan.steps.size() == 3)
    {
        const ExecutionPlanStep& step1 =
            result.executionPlan.steps[0];

        const ExecutionPlanStep& step2 =
            result.executionPlan.steps[1];

        const ExecutionPlanStep& step3 =
            result.executionPlan.steps[2];

           cout << "[RESOLVED VALUES]"
               << endl;
           cout << "Step 2 location: "
               << step2.operation.parameters.location
               << endl;
           cout << "Step 2 content: "
               << (step2.operation.parameters.content.empty()
                    ? "[none]"
                    : "[present]")
               << endl;
           cout << "Step 3 target: "
               << step3.operation.parameters.target
               << endl;

        testsPassed &=
            assertCondition(
                step1.id == "step_1",
                "Step 1 order is correct"
            );

        testsPassed &=
            assertCondition(
                step1.operation.name ==
                "filesystem.create",
                "Step 1 primitive is filesystem.create"
            );

        testsPassed &=
            assertCondition(
                step1.operation.parameters.name ==
                "NovaJava",
                "Step 1 preserves NovaJava"
            );

        testsPassed &=
            assertCondition(
                step2.id == "step_2",
                "Step 2 order is correct"
            );

        testsPassed &=
            assertCondition(
                step2.operation.name ==
                "filesystem.create_file",
                "Step 2 primitive is filesystem.create_file"
            );

        testsPassed &=
            assertCondition(
                step2.operation.parameters.name ==
                "Main.java",
                "Step 2 preserves Main.java"
            );

        testsPassed &=
            assertCondition(
                step2.operation.parameters.location ==
                "desktop/NovaJava",
                "Step 2 context location is resolved"
            );

        testsPassed &=
            assertCondition(
                !step2.operation.parameters.content.empty(),
                "Step 2 content is resolved"
            );

        testsPassed &=
            assertCondition(
                step3.id == "step_3",
                "Step 3 order is correct"
            );

        testsPassed &=
            assertCondition(
                step3.operation.name ==
                "application.launch",
                "Step 3 primitive is application.launch"
            );

        testsPassed &=
            assertCondition(
                step3.operation.parameters.application ==
                "VS Code",
                "Step 3 application is VS Code"
            );

        testsPassed &=
            assertCondition(
                step3.operation.parameters.target ==
                "desktop/NovaJava",
                "Step 3 target is resolved"
            );

        testsPassed &=
            assertCondition(
                step1.valid &&
                step2.valid &&
                step3.valid,
                "All execution-plan steps are valid"
            );
    }

    cout << endl;

    cout << "[B3 INTEGRATION RESULT]"
         << endl;

    cout << "Status: "
         << (testsPassed ? "PASS" : "FAIL")
         << endl;

    if (!testsPassed)
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