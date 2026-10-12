#include "execution_plan_builder.hpp"

#include <unordered_map>

using namespace std;

ExecutionPlan ExecutionPlanBuilder::build(
    const Action& action,
    const Capability& capability,
    const ActionParameters& parameters,
    const ActionValidationResult& validation
)
{
    ExecutionPlan plan;

    plan.valid = false;
    plan.type = "";
    plan.action.name = "";
    plan.capability.supported = false;
    plan.capability.name = "";
    plan.capability.message = "";

    plan.parameters = PrimitiveParameters{};

    plan.message = "";

    // --------------------------------------------------------
    // VALIDATION GATE
    // --------------------------------------------------------

    if (!validation.valid)
    {
        plan.message =
            "Cannot build execution plan from invalid action.";

        return plan;
    }

    // --------------------------------------------------------
    // BUILD PLAN
    // --------------------------------------------------------

    plan.type =
        "single_action";

    plan.action =
        action;

    plan.capability =
        capability;

    const auto copyLiteral = [](
        const ParameterValue& source,
        string& destination)
    {
        if (source.type == ParameterValueType::NONE)
        {
            destination.clear();
            return true;
        }

        if (source.type != ParameterValueType::LITERAL ||
            source.value.empty())
        {
            return false;
        }

        destination = source.value;
        return true;
    };

    if (!copyLiteral(parameters.name, plan.parameters.name) ||
        !copyLiteral(parameters.location, plan.parameters.location) ||
        !copyLiteral(parameters.application, plan.parameters.application) ||
        !copyLiteral(parameters.target, plan.parameters.target) ||
        !copyLiteral(parameters.content, plan.parameters.content))
    {
        plan.message =
            "Cannot build execution plan with unresolved semantic parameters.";
        return plan;
    }

    plan.valid = true;

    plan.message =
        "Execution plan built successfully.";
    return plan;
}

ExecutionPlan ExecutionPlanBuilder::build(
    const MultiStepPlan& plan,
    const vector<string>& executionOrder
)
{
    ExecutionPlan executionPlan;

    executionPlan.valid = false;
    executionPlan.type = "multi_step";
    executionPlan.steps.clear();
    executionPlan.message = "";

    // --------------------------------------------------------
    // INPUT VALIDATION
    // --------------------------------------------------------

    if (!plan.valid)
    {
        executionPlan.message =
            "Cannot build execution plan from invalid multi-step plan.";

        return executionPlan;
    }

    if (plan.steps.empty())
    {
        executionPlan.message =
            "Cannot build execution plan from empty multi-step plan.";

        return executionPlan;
    }

    if (executionOrder.empty())
    {
        executionPlan.message =
            "Cannot build execution plan without execution order.";

        return executionPlan;
    }

    if (
        executionOrder.size() !=
        plan.steps.size()
    )
    {
        executionPlan.message =
            "Execution order does not contain every plan step.";

        return executionPlan;
    }

    // --------------------------------------------------------
    // STEP LOOKUP
    // --------------------------------------------------------

    unordered_map<string, const MultiStepAction*> stepMap;

    for (const auto& step : plan.steps)
    {
        if (step.id.empty())
        {
            executionPlan.message =
                "Cannot build execution plan from step with empty ID.";

            return executionPlan;
        }

        if (stepMap.contains(step.id))
        {
            executionPlan.message =
                "Duplicate step ID in execution plan: " +
                step.id;

            return executionPlan;
        }

        stepMap[step.id] =
            &step;
    }

    // --------------------------------------------------------
    // BUILD ORDERED EXECUTION STEPS
    // --------------------------------------------------------

    for (const string& stepId :
         executionOrder)
    {
        if (!stepMap.contains(stepId))
        {
            executionPlan.message =
                "Execution order references unknown step: " +
                stepId;

            executionPlan.steps.clear();

            return executionPlan;
        }

        const MultiStepAction& sourceStep =
            *stepMap[stepId];

        if (sourceStep.operations.empty())
        {
            executionPlan.message =
                "Step " +
                stepId +
                " contains no executable operations.";

            executionPlan.steps.clear();

            return executionPlan;
        }

        for (const PrimitiveOperation& operation :
             sourceStep.operations)
        {
            if (!operation.resolved)
            {
                executionPlan.message =
                    "Step " +
                    stepId +
                    " contains an unresolved primitive: " +
                    operation.name;

                executionPlan.steps.clear();

                return executionPlan;
            }

            ExecutionPlanStep executionStep;

            executionStep.id =
                stepId;

            executionStep.dependencies =
                sourceStep.dependencies;

            executionStep.operation =
                operation;

            executionStep.valid =
                true;

            executionPlan.steps.push_back(
                executionStep
            );
        }
    }

    // --------------------------------------------------------
    // FINAL VALIDATION
    // --------------------------------------------------------

    if (executionPlan.steps.empty())
    {
        executionPlan.message =
            "Execution plan contains no executable steps.";

        return executionPlan;
    }

    executionPlan.valid =
        true;

    executionPlan.message =
        "Multi-step execution plan built successfully.";

    return executionPlan;
}