#include <iostream>

#include "multistep_pipeline.hpp"
#include "multi_step_executor.hpp"

using namespace std;

int main()
{
    cout << "=== Multi-Step B3 → Part C Integration Test ==="
         << endl;

    const string goal =
        "Create a folder named NovaJava on my desktop, "
        "create a file named Main.java inside that folder, "
        "then open the NovaJava folder in VS Code.";

    // --------------------------------------------------
    // Step 1: Run B3 planning pipeline
    // --------------------------------------------------

    MultiStepPipeline pipeline;

    MultiStepPipelineResult planningResult =
        pipeline.run(goal);

    if (
        planningResult.status
        !=
        MultiStepPipelineStatus::SUCCESS
    )
    {
        cout
            << "[ASSERT] B3 pipeline : FAIL"
            << endl;

        cout
            << "[MESSAGE] "
            << planningResult.message
            << endl;

        return 1;
    }

    cout
        << "[ASSERT] B3 pipeline : PASS"
        << endl;

    // --------------------------------------------------
    // Step 2: Validate generated execution plan
    // --------------------------------------------------

    const ExecutionPlan& executionPlan =
        planningResult.executionPlan;

    cout
        << "[EXECUTION PLAN] "
        << executionPlan.type
        << endl;

    cout
        << "[EXECUTION PLAN STEPS] "
        << executionPlan.steps.size()
        << endl;

    if (!executionPlan.valid)
    {
        cout
            << "[ASSERT] Execution plan valid : FAIL"
            << endl;

        return 1;
    }

    cout
        << "[ASSERT] Execution plan valid : PASS"
        << endl;

    // --------------------------------------------------
    // Step 3: Execute actual plan
    // --------------------------------------------------

    MultiStepExecutor executor;

    MultiStepExecutionResult executionResult =
        executor.execute(executionPlan);

    cout
        << "[EXECUTION SUCCESS] "
        << boolalpha
        << executionResult.success
        << endl;

    cout
        << "[EXECUTION MESSAGE] "
        << executionResult.message
        << endl;

    cout
        << "[STEP RESULTS] "
        << executionResult.stepResults.size()
        << endl;

    // --------------------------------------------------
    // Step 4: Assertions
    // --------------------------------------------------

    bool success = true;

    if (executionResult.success)
    {
        cout
            << "[ASSERT] Overall execution : PASS"
            << endl;
    }
    else
    {
        cout
            << "[ASSERT] Overall execution : FAIL"
            << endl;

        success = false;
    }

    if (
        executionResult.stepResults.size()
        ==
        executionPlan.steps.size()
    )
    {
        cout
            << "[ASSERT] Result count matches plan : PASS"
            << endl;
    }
    else
    {
        cout
            << "[ASSERT] Result count matches plan : FAIL"
            << endl;

        success = false;
    }

    for (
        size_t i = 0;
        i < executionResult.stepResults.size();
        ++i
    )
    {
        const ExecutionResult& stepResult =
            executionResult.stepResults[i];

        const bool stepSuccess =
            stepResult.status
            ==
            ExecutionStatus::SUCCESS;

        cout
            << "[ASSERT] Step "
            << (i + 1)
            << " : "
            << (stepSuccess ? "PASS" : "FAIL")
            << endl;

        if (!stepSuccess)
        {
            cout
                << "          "
                << stepResult.message
                << endl;

            success = false;
        }
    }

    return success ? 0 : 1;
}