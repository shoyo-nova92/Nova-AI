#include <iostream>

#include "multi_step_executor.hpp"

using namespace std;

int main()
{
    cout << "=== MultiStepExecutor Test ===" << endl;

    ExecutionPlan plan{};

    plan.valid = true;
    plan.type = "multi_step";

    // --------------------------------------------------
    // Step 1
    // --------------------------------------------------

    ExecutionPlanStep step1{};

    step1.id = "step_1";
    step1.dependencies = {};
    step1.valid = true;

    step1.operation.type =
        PrimitiveOperationType::FILESYSTEM_CREATE;

    step1.operation.name =
        "filesystem.create";

    step1.operation.parameters.name =
        "NovaMultiStepTest";

    step1.operation.parameters.location =
        "desktop";

    step1.operation.resolved = true;

    // --------------------------------------------------
    // Step 2
    // --------------------------------------------------

    ExecutionPlanStep step2{};

    step2.id = "step_2";
    step2.dependencies = {"step_1"};
    step2.valid = true;

    step2.operation.type =
        PrimitiveOperationType::FILESYSTEM_CREATE;

    step2.operation.name =
        "filesystem.create";

    step2.operation.parameters.name =
        "SecondFolder";

    step2.operation.parameters.location =
        "desktop";

    step2.operation.resolved = true;

    // --------------------------------------------------
    // Add steps
    // --------------------------------------------------

    plan.steps.push_back(step1);
    plan.steps.push_back(step2);

    // --------------------------------------------------
    // Execute
    // --------------------------------------------------

    MultiStepExecutor executor;

    MultiStepExecutionResult result =
        executor.execute(plan);

    // --------------------------------------------------
    // Output
    // --------------------------------------------------

    cout
        << "[SUCCESS] "
        << boolalpha
        << result.success
        << endl;

    cout
        << "[STEP RESULTS] "
        << result.stepResults.size()
        << endl;

    cout
        << "[MESSAGE] "
        << result.message
        << endl;

    // --------------------------------------------------
    // Assertions
    // --------------------------------------------------

    bool success = true;

    if (!result.success)
    {
        cout
            << "[ASSERT] Overall execution : FAIL"
            << endl;

        success = false;
    }
    else
    {
        cout
            << "[ASSERT] Overall execution : PASS"
            << endl;
    }

    if (result.stepResults.size() != 2)
    {
        cout
            << "[ASSERT] Two step results : FAIL"
            << endl;

        success = false;
    }
    else
    {
        cout
            << "[ASSERT] Two step results : PASS"
            << endl;
    }

    for (size_t i = 0; i < result.stepResults.size(); ++i)
    {
        const bool stepSuccess =
            result.stepResults[i].status
            ==
            ExecutionStatus::SUCCESS;

        cout
            << "[ASSERT] Step "
            << (i + 1)
            << " success : "
            << (stepSuccess ? "PASS" : "FAIL")
            << endl;

        if (!stepSuccess)
        {
            success = false;
        }
    }

    return success ? 0 : 1;
}