#include "plan_validator.hpp"

PlanValidationResult PlanValidator::validate(
    const MultiStepPlan& plan
)
{
    PlanValidationResult result;

    result.status =
        PlanValidationStatus::INVALID;

    if (!plan.valid)
    {
        result.message =
            "Plan is marked invalid.";

        return result;
    }

    if (plan.steps.empty())
    {
        result.message =
            "Plan contains no steps.";

        return result;
    }

    for (const MultiStepAction& step : plan.steps)
    {
        if (step.id.empty())
        {
            result.message =
                "Plan contains a step with no ID.";

            return result;
        }

        if (step.operations.empty())
        {
            result.message =
                "Step " + step.id +
                " contains no operations.";

            return result;
        }

        for (const PrimitiveOperation& operation :
             step.operations)
        {
            if (!operation.resolved)
            {
                result.message =
                    "Unresolved primitive in step " +
                    step.id + ": " +
                    operation.name;

                return result;
            }
        }

        for (const ContextReference& reference :
             step.contextReferences)
        {
            if (!reference.resolved)
            {
                result.message =
                    "Unresolved context reference in step " +
                    step.id;

                return result;
            }
        }
    }

    result.status =
        PlanValidationStatus::VALID;

    result.message =
        "Plan passed structural validation.";

    return result;
}