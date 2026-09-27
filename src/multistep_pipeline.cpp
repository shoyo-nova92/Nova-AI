#include "multistep_pipeline.hpp"
#include <iostream>

MultiStepPipelineResult MultiStepPipeline::run(
    const string& goal
)
{
    MultiStepPipelineResult result;

    result.status =
        MultiStepPipelineStatus::FAILED;

    // ============================================================
    // 1. GOAL DECOMPOSITION
    // ============================================================

    GoalDecomposer goalDecomposer;

    MultiStepPlan plan =
        goalDecomposer.decompose(
            goal
        );

    if (!plan.valid)
    {
        result.message =
            "Goal decomposition failed: " +
            plan.message;

        return result;
    }
    cout << "\n[DECOMPOSED STEPS]\n";
    for (const MultiStepAction& step : plan.steps)
    {
        cout << "Step ID: "
            << step.id
            << endl;

        cout << "Description: "
            << step.description
            << endl;

        cout << "Dependencies: ";

        for (const string& dependency : step.dependencies)
        {
            cout << dependency << " ";
        }

        cout << endl;

        cout << "------------------------"
            << endl;
    }

    // ============================================================
    // 2. DEPENDENCY RESOLUTION
    // ============================================================

    DependencyResolver dependencyResolver;

    DependencyResolutionResult dependencyResult =
        dependencyResolver.resolve(
            plan
        );

    if (!dependencyResult.valid)
    {
        result.message =
            "Dependency resolution failed: " +
            dependencyResult.message;

        return result;
    }

    // ============================================================
    // 3. ACTION / PRIMITIVE RESOLUTION
    // ============================================================

    ActionPrimitiveResolver actionPrimitiveResolver;

    for (MultiStepAction& step : plan.steps)
    {
        ActionPrimitiveResolutionResult primitiveResult =
            actionPrimitiveResolver.resolve(
                step
            );

        if (
            primitiveResult.status !=
            ActionPrimitiveResolutionStatus::RESOLVED
        )
        {
            result.message =
                "Action / primitive resolution failed for " +
                step.id +
                ": " +
                primitiveResult.message;

            if (
                primitiveResult.status ==
                ActionPrimitiveResolutionStatus::CLARIFICATION_REQUIRED
            )
            {
                result.message +=
                    " Clarification: " +
                    primitiveResult.clarificationQuestion;
            }

            return result;
        }

                step =
                    primitiveResult.action;

                cout << "\n[PRIMITIVE RESOLUTION] "
                    << step.id
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
                }
            }
    // ============================================================
    // 4. CAPABILITY + PARAMETER RESOLUTION
    // ============================================================

    CapabilityResolver capabilityResolver;

    ParameterResolver parameterResolver;

    for (MultiStepAction& step : plan.steps)
    {
        for (PrimitiveOperation& operation : step.operations)
        {
            Capability capability =
                capabilityResolver.resolve(
                    operation
                );

            if (!capability.supported)
            {
                result.message =
                    "Unsupported capability for " +
                    operation.name +
                    ": " +
                    capability.message;

                return result;
            }

            ParameterResolverResult parameterResult =
                parameterResolver.resolve(
                    step.description,
                    operation,
                    capability
                );

            if (
                parameterResult.status ==
                ParameterResolutionStatus::CLARIFICATION_REQUIRED
            )
            {
                result.status =
                    MultiStepPipelineStatus::CLARIFICATION_REQUIRED;

                result.message =
                    "Parameter resolution requires clarification for " +
                    step.id +
                    ": " +
                    parameterResult.message;

                result.clarificationQuestion =
                    parameterResult.clarificationQuestion;

                return result;
            }

            if (
                parameterResult.status ==
                ParameterResolutionStatus::FAILED
            )
            {
                result.status =
                    MultiStepPipelineStatus::FAILED;

                result.message =
                    "Parameter resolution failed for " +
                    operation.name +
                    " in " +
                    step.id +
                    ": " +
                    parameterResult.message;

                return result;
            }

            operation.parameters =
                parameterResolver.toPrimitiveParameters(
                    parameterResult.parameters
                );
        }
    }

    // ============================================================
    // 5. CONTEXT RESOLUTION
    // ============================================================

    ContextResolver contextResolver;

    for (size_t i = 0; i < plan.steps.size(); ++i)
    {
        MultiStepAction& step =
            plan.steps[i];

        MultiStepPlan contextPlan =
            plan;

        ContextResolutionResult contextResult =
            contextResolver.resolve(
                step,
                contextPlan
            );

        if (
            contextResult.status ==
            ContextResolutionStatus::CLARIFICATION_REQUIRED
        )
        {
            result.message =
                "Context resolution requires clarification: " +
                contextResult.clarificationQuestion;

            return result;
        }

        if (
            contextResult.status ==
            ContextResolutionStatus::FAILED
        )
        {
            result.message =
                "Context resolution failed for " +
                step.id +
                ": " +
                contextResult.message;

            return result;
        }

        step =
            contextResult.action;
    }

    // ============================================================
    // 6. FINAL RESULT
    // ============================================================

    plan.valid =
        true;

    result.status =
        MultiStepPipelineStatus::SUCCESS;

    result.plan =
        plan;

    result.message =
        "Multi-step plan resolved successfully.";

    return result;
}