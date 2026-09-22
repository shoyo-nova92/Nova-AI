#include "single_action_pipeline.hpp"

#include "capability_resolver.hpp"
#include "parameter_resolver.hpp"
#include "action_validator.hpp"
#include "execution_plan_builder.hpp"

using namespace std;

SingleActionResult SingleActionPipeline::process(
    const string& input
)
{
    SingleActionResult result;

    result.success = false;

    result.action.name = "";

    result.capability.supported = false;
    result.capability.name = "";
    result.capability.message = "";

    result.parameters.name = "";
    result.parameters.location = "";
    result.parameters.application = "";

    result.validation.valid = false;
    result.validation.message = "";

    result.executionPlan.valid = false;
    result.executionPlan.type = "";
    result.executionPlan.action.name = "";
    result.executionPlan.capability.supported = false;
    result.executionPlan.capability.name = "";
    result.executionPlan.capability.message = "";
    result.executionPlan.parameters.name = "";
    result.executionPlan.parameters.location = "";
    result.executionPlan.parameters.application = "";
    result.executionPlan.message = "";

    result.message = "";

    // --------------------------------------------------------
    // STAGE 1 — ACTION PARSER
    // --------------------------------------------------------

    ActionParser actionParser;

    ActionParserResult actionResult =
        actionParser.parse(input);

    if (!actionResult.success)
    {
        result.message =
            actionResult.message;

        return result;
    }

    result.action =
        actionResult.action;

    // --------------------------------------------------------
    // STAGE 2 — CAPABILITY RESOLVER
    // --------------------------------------------------------

    CapabilityResolver capabilityResolver;

    Capability capability =
        capabilityResolver.resolve(
            result.action
        );

    if (!capability.supported)
    {
        result.message =
            capability.message;

        return result;
    }

    result.capability =
        capability;

    // --------------------------------------------------------
    // STAGE 3 — PARAMETER RESOLVER
    // --------------------------------------------------------

    ParameterResolver parameterResolver;

    ParameterResolverResult parameterResult =
        parameterResolver.resolve(
            input,
            result.action,
            result.capability
        );

    if (!parameterResult.success)
    {
        result.message =
            parameterResult.message;

        return result;
    }

    result.parameters =
        parameterResult.parameters;

    // --------------------------------------------------------
    // STAGE 4 — ACTION VALIDATOR
    // --------------------------------------------------------

    ActionValidator actionValidator;

    ActionValidationResult validationResult =
        actionValidator.validate(
            result.action,
            result.capability,
            result.parameters
        );

    result.validation =
        validationResult;

    if (!validationResult.valid)
    {
        result.message =
            validationResult.message;

        return result;
    }

    // --------------------------------------------------------
    // STAGE 5 — EXECUTION PLAN
    // --------------------------------------------------------

    ExecutionPlanBuilder executionPlanBuilder;

    ExecutionPlan executionPlan =
        executionPlanBuilder.build(
            result.action,
            result.capability,
            result.parameters,
            result.validation
        );

    result.executionPlan =
        executionPlan;

    if (!executionPlan.valid)
    {
        result.message =
            executionPlan.message;

        return result;
    }

    // --------------------------------------------------------
    // PIPELINE SUCCESS
    // --------------------------------------------------------

    result.success = true;

    result.message =
        "Single action execution plan created successfully.";

    return result;
}