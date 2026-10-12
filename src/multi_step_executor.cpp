#include "multi_step_executor.hpp"
#include "primitive_execution_engine.hpp"

#include <iostream>
#include <unordered_map>

using namespace std;

MultiStepExecutionResult MultiStepExecutor::execute(
    const ExecutionPlan& plan
)
{
    MultiStepExecutionResult result{};

    result.success = false;

    // --------------------------------------------------
    // Step 1: Validate execution plan
    // --------------------------------------------------

    if (!plan.valid)
    {
        result.message =
            "Multi-step execution plan is invalid.";

        return result;
    }

    if (plan.type != "multi_step")
    {
        result.message =
            "Execution plan is not a multi-step plan.";

        return result;
    }

    if (plan.steps.empty())
    {
        result.message =
            "Multi-step execution plan contains no steps.";

        return result;
    }

    // --------------------------------------------------
    // Step 2: Track completed steps
    // --------------------------------------------------

    unordered_map<string, bool> completedSteps;

    PrimitiveExecutionEngine primitiveEngine;

    // --------------------------------------------------
    // Step 3: Execute steps in execution-plan order
    // --------------------------------------------------

    for (const ExecutionPlanStep& step : plan.steps)
    {
        cout
            << "[MULTI-STEP EXECUTION] "
            << step.id
            << endl;

        // ----------------------------------------------
        // Validate current step
        // ----------------------------------------------

        if (!step.valid)
        {
            result.message =
                "Step is invalid: "
                + step.id;

            return result;
        }

        // ----------------------------------------------
        // Check dependencies
        // ----------------------------------------------

        for (const string& dependency : step.dependencies)
        {
            auto dependencyResult =
                completedSteps.find(dependency);

            if (
                dependencyResult ==
                completedSteps.end()
                ||
                !dependencyResult->second
            )
            {
                result.message =
                    "Dependency was not successfully completed for step: "
                    + step.id;

                return result;
            }
        }

        // ----------------------------------------------
        // Execute primitive
        // ----------------------------------------------
        cout
            << "[PRIMITIVE PARAMETERS]"
            << endl;

        cout
            << "  Name: "
            << step.operation.parameters.name
            << endl;

        cout
            << "  Location: "
            << step.operation.parameters.location
            << endl;

        cout
            << "  Application: "
            << step.operation.parameters.application
            << endl;

        cout
            << "  Target: "
            << step.operation.parameters.target
            << endl;

        cout
            << "  Content: "
            << step.operation.parameters.content
            << endl;

        ExecutionResult executionResult =
            primitiveEngine.execute(
                step.operation
            );

        result.stepResults.push_back(
            executionResult
        );

        cout
            << "[STEP RESULT] "
            << executionResult.message
            << endl;

        // ----------------------------------------------
        // Stop immediately on failure
        // ----------------------------------------------

        if (
            executionResult.status
            !=
            ExecutionStatus::SUCCESS
        )
        {
            completedSteps[step.id] = false;

            result.message =
                "Multi-step execution stopped at step: "
                + step.id;

            return result;
        }

        completedSteps[step.id] = true;
    }

    // --------------------------------------------------
    // Step 4: All steps completed
    // --------------------------------------------------

    result.success = true;

    result.message =
        "All multi-step operations executed successfully.";

    return result;
}