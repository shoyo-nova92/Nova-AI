#include "execution_plan_builder.hpp"

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

    plan.parameters.name = "";
    plan.parameters.location = "";
    plan.parameters.application = "";

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

    plan.parameters =
        parameters;

    plan.valid = true;

    plan.message =
        "Execution plan built successfully.";

    return plan;
}