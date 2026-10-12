#include "multistep_pipeline.hpp"
#include "plan_validator.hpp"
#include "execution_plan_builder.hpp"

#include <algorithm>
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
    plan =
        dependencyResult.plan;

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
        step.semanticParameters.clear();
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

            vector<pair<string, string>> availableContext;
            for (const MultiStepAction& previousStep : plan.steps)
            {
                if (find(
                        step.dependencies.begin(),
                        step.dependencies.end(),
                        previousStep.id) != step.dependencies.end())
                {
                    availableContext.emplace_back(
                        previousStep.id,
                        previousStep.description);
                }
            }

            ParameterResolverResult parameterResult =
                parameterResolver.resolve(
                    step.description,
                    operation,
                    capability,
                    availableContext
                );

            if (
                parameterResult.status ==
                    ParameterResolutionStatus::AMBIGUOUS ||
                parameterResult.status ==
                    ParameterResolutionStatus::MISSING_REQUIRED_PARAMETER
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

            if (!parameterResult.success)
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

            step.semanticParameters.push_back(
                parameterResult.parameters);
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

    PlanValidator planValidator;

    PlanValidationResult validationResult =
        planValidator.validate(plan);

    if (validationResult.status ==
        PlanValidationStatus::INVALID)
    {
        result.status =
            MultiStepPipelineStatus::FAILED;

        result.message =
            "Plan validation failed: " +
            validationResult.message;

        return result;
    }

    cout << "\n[PLAN VALIDATION]\n";
    cout << "Status: VALID" << endl;
    cout << "Message: "
        << validationResult.message
        << endl;

    // --------------------------------------------------
    // EXECUTION PLAN
    // --------------------------------------------------

    ExecutionPlanBuilder executionPlanBuilder;

    ExecutionPlan executionPlan =
        executionPlanBuilder.build(
            plan,
            dependencyResult.executionOrder
        );
    
    result.executionPlan = executionPlan;
        
    if (!executionPlan.valid)
    {
        result.status =
            MultiStepPipelineStatus::FAILED;

        result.message =
            "Execution plan construction failed: " +
            executionPlan.message;

        return result;
    }

    cout << "\n[EXECUTION PLAN]\n";
    cout << "Type: "
        << executionPlan.type
        << endl;

    cout << "Steps: "
        << executionPlan.steps.size()
        << endl;

    for (const ExecutionPlanStep& step :
        executionPlan.steps)
    {
        cout << "  Step: "
            << step.id
            << endl;

        cout << "  Primitive: "
            << step.operation.name
            << endl;

        cout << "  Valid: "
            << (step.valid ? "true" : "false")
            << endl;

        cout << "------------------------"
            << endl;
    }
        
    result.plan =
        plan;

    result.message =
        "Multi-step plan resolved successfully.";

    return result;
}