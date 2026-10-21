#include "context_resolver.hpp"
#include "filesystem_path_policy.hpp"

namespace
{
    bool createdResourcePath(
        const PrimitiveOperation& operation,
        string& value)
    {
        if (operation.name != "filesystem.create" ||
            operation.parameters.name.empty() ||
            operation.parameters.location.empty())
        {
            return false;
        }

        FilesystemPathPolicy policy;
        const FilesystemPathResult parent =
            policy.resolve(operation.parameters.location);
        if (!parent.ok)
        {
            return false;
        }

        // The executor will authorize and create this target. Context
        // resolution only carries a normalized path; it never manufactures one
        // by concatenating strings.
        value = (parent.path / operation.parameters.name)
            .lexically_normal().string();
        return true;
    }

    bool resolveOutput(
        const MultiStepAction& sourceStep,
        const string& outputName,
        string& value
    )
    {
        for (const PrimitiveOperation& operation : sourceStep.operations)
        {
            const PrimitiveParameters& parameters = operation.parameters;

            if (outputName == "name" && !parameters.name.empty())
            {
                value = parameters.name;
                return true;
            }
            if (outputName == "location" && !parameters.location.empty())
            {
                value = parameters.location;
                return true;
            }
            if (outputName == "application" && !parameters.application.empty())
            {
                value = parameters.application;
                return true;
            }
            if (outputName == "target" && !parameters.target.empty())
            {
                value = parameters.target;
                return true;
            }
            if (outputName == "content" && !parameters.content.empty())
            {
                value = parameters.content;
                return true;
            }
            if (outputName == "path")
            {
                string createdPath;
                if (createdResourcePath(operation, createdPath))
                {
                    value = createdPath;
                    return true;
                }
                if (!parameters.target.empty())
                {
                    value = parameters.target;
                    return true;
                }
            }
        }

        return false;
    }

    ParameterValue makeLiteral(const string& value)
    {
        return ParameterValue{
            ParameterValueType::LITERAL,
            value,
            ContextParameterReference{}
        };
    }

    bool convertToPrimitiveParameters(
        const ActionParameters& parameters,
        PrimitiveParameters& primitiveParameters,
        string& errorMessage
    )
    {
        const auto convert = [&errorMessage](
            const ParameterValue& source,
            string& destination,
            const string& name)
        {
            if (source.type == ParameterValueType::NONE)
            {
                destination.clear();
                return true;
            }

            if (source.type != ParameterValueType::LITERAL ||
                source.value.empty())
            {
                errorMessage =
                    "Parameter is not a concrete execution value: " + name;
                return false;
            }

            destination = source.value;
            return true;
        };

        return
            convert(parameters.name, primitiveParameters.name, "name") &&
            convert(parameters.location, primitiveParameters.location, "location") &&
            convert(parameters.application, primitiveParameters.application, "application") &&
            convert(parameters.target, primitiveParameters.target, "target") &&
            convert(parameters.content, primitiveParameters.content, "content");
    }
}

ContextResolutionResult ContextResolver::resolve(
    const MultiStepAction& action,
    const MultiStepPlan& plan
)
{
    if (!action.semanticParameters.empty())
    {
        ContextResolutionResult semanticResult;
        semanticResult.action = action;

        if (action.semanticParameters.size() != action.operations.size())
        {
            semanticResult.status = ContextResolutionStatus::FAILED;
            semanticResult.message =
                "Semantic parameter count does not match primitive count.";
            return semanticResult;
        }

        size_t currentIndex = plan.steps.size();
        for (size_t index = 0; index < plan.steps.size(); ++index)
        {
            if (plan.steps[index].id == action.id)
            {
                currentIndex = index;
                break;
            }
        }

        MultiStepAction resolvedAction = action;
        for (size_t operationIndex = 0;
             operationIndex < resolvedAction.operations.size();
             ++operationIndex)
        {
            ActionParameters& parameters =
                resolvedAction.semanticParameters[operationIndex];

            const auto resolveParameter = [&](ParameterValue& parameter)
            {
                if (parameter.type != ParameterValueType::CONTEXT_REFERENCE)
                {
                    return true;
                }

                size_t sourceIndex = plan.steps.size();
                const MultiStepAction* sourceStep = nullptr;
                for (size_t index = 0; index < plan.steps.size(); ++index)
                {
                    if (plan.steps[index].id == parameter.reference.stepId)
                    {
                        sourceIndex = index;
                        sourceStep = &plan.steps[index];
                        break;
                    }
                }

                string resolvedValue;
                if (currentIndex == plan.steps.size() ||
                    sourceIndex >= currentIndex ||
                    sourceStep == nullptr ||
                    !resolveOutput(
                        *sourceStep,
                        parameter.reference.output,
                        resolvedValue))
                {
                    return false;
                }

                const string referenceName =
                    parameter.reference.stepId + "." +
                    parameter.reference.output;
                resolvedAction.contextReferences.push_back(
                    ContextReference{
                        ContextReferenceType::PREVIOUS_STEP_OUTPUT,
                        referenceName,
                        parameter.reference.stepId,
                        true,
                        resolvedValue
                    });
                semanticResult.resolvedContext[referenceName] = resolvedValue;
                parameter = makeLiteral(resolvedValue);
                return true;
            };

            if (!resolveParameter(parameters.name) ||
                !resolveParameter(parameters.location) ||
                !resolveParameter(parameters.application) ||
                !resolveParameter(parameters.target) ||
                !resolveParameter(parameters.content))
            {
                semanticResult.status =
                    ContextResolutionStatus::CLARIFICATION_REQUIRED;
                semanticResult.clarificationQuestion =
                    "A referenced previous-step output could not be resolved.";
                semanticResult.message =
                    "Semantic context reference could not be resolved.";
                return semanticResult;
            }

            PrimitiveParameters primitiveParameters;
            string conversionError;
                if (!convertToPrimitiveParameters(
                    parameters,
                    primitiveParameters,
                    conversionError))
            {
                semanticResult.status = ContextResolutionStatus::FAILED;
                semanticResult.message = conversionError;
                return semanticResult;
            }

            resolvedAction.operations[operationIndex].parameters =
                primitiveParameters;
        }

        semanticResult.status = ContextResolutionStatus::RESOLVED;
        semanticResult.action = resolvedAction;
        semanticResult.message = "Semantic parameters resolved successfully.";
        return semanticResult;
    }

    ContextResolutionResult result;

    result.action =
        action;

    // --------------------------------------------------------
    // NO CONTEXT REFERENCE
    // --------------------------------------------------------

    if (
        action.dependencies.empty() &&
        !containsContextReference(action.description)
    )
    {
        result.status = ContextResolutionStatus::RESOLVED;
        result.message =
            "No contextual references require resolution.";
        return result;
    }

    // --------------------------------------------------------
    // RESOLVE PREVIOUS-STEP CONTEXT
    // --------------------------------------------------------

    MultiStepAction resolvedAction =
        action;

    bool resolved =
        resolvePreviousStepContext(
            resolvedAction,
            plan
        );

    if (resolved)
    {
        result.status =
            ContextResolutionStatus::RESOLVED;

        result.action =
            resolvedAction;

        for (
            const ContextReference& reference
            : resolvedAction.contextReferences
        )
        {
            if (reference.resolved)
            {
                result.resolvedContext[
                    reference.reference
                ] =
                    reference.resolvedValue;

                result.resolvedContext[action.description] =
                    reference.resolvedValue;
            }
        }

        result.message =
            "Context resolved successfully.";

        return result;
    }

    // --------------------------------------------------------
    // CONTEXT COULD NOT BE RESOLVED
    // --------------------------------------------------------

    result.status =
        ContextResolutionStatus::CLARIFICATION_REQUIRED;

    result.clarificationQuestion =
        "I could not determine which previous resource this reference refers to.";

    result.message =
        "Context reference could not be resolved.";

    return result;
}

bool ContextResolver::containsContextReference(
    const string& description
)
{
    const string text = description;

    return
        text.find("that folder") != string::npos ||
        text.find("the folder") != string::npos ||
        text.find("that file") != string::npos ||
        text.find("the file") != string::npos ||
        text.find("inside the ") != string::npos ||
        text.find("within the ") != string::npos ||
        text.find("in the project directory") != string::npos ||
        text.find("project directory in ") != string::npos ||
        text.find(" project in ") != string::npos;
}

bool ContextResolver::resolvePreviousStepContext(
    MultiStepAction& action,
    const MultiStepPlan& plan
)
{
    if (
        action.dependencies.empty() &&
        !containsContextReference(action.description)
    )
    {
        return true;
    }
    // --------------------------------------------------------
    // Search dependency ancestry for the resource.
    // --------------------------------------------------------

    vector<string> pendingDependencies =
        action.dependencies;

    while (!pendingDependencies.empty())
    {
        string dependencyId =
            pendingDependencies.back();

        pendingDependencies.pop_back();
        for (const MultiStepAction& previousStep : plan.steps)
        {
            if (previousStep.id != dependencyId)
            {
                continue;
            }

            // ------------------------------------------------
            // First inspect this dependency's operations.
            // ------------------------------------------------

            for (
                const PrimitiveOperation& operation
                : previousStep.operations
            )
            {
                if (
                    operation.name ==
                    "filesystem.create"
                    &&
                    operation.resolved
                )
                {
                    const string& name =
                        operation.parameters.name;

                    const string& location =
                        operation.parameters.location;

                    if (
                        name.empty() ||
                        location.empty()
                    )
                    {
                        continue;
                    }

                    string resolvedPath;
                    if (!createdResourcePath(operation, resolvedPath))
                    {
                        continue;
                    }

                    ContextReference reference
                    {
                        ContextReferenceType::PREVIOUS_STEP_LOCATION,
                        action.description,
                        previousStep.id,
                        true,
                        resolvedPath
                    };

                    action.contextReferences.push_back(
                        reference
                    );

                    // ------------------------------------------------
                    // Apply resolved project resource to dependent
                    // operations.
                    // ------------------------------------------------

                    for (
                        PrimitiveOperation& dependentOperation
                        : action.operations
                    )
                    {
                        // ------------------------------------------------
                        // A file being created "inside" the project
                        // receives the resolved project path as location.
                        // ------------------------------------------------

                        if (
                            dependentOperation.name ==
                            "filesystem.create_file"
                        )
                        {
                            dependentOperation.parameters.location =
                                resolvedPath;
                        }

                        // ------------------------------------------------
                        // Text written into a resource uses the resolved
                        // project path when the target refers to it.
                        // ------------------------------------------------

                        if (
                            dependentOperation.name ==
                            "text.write"
                        )
                        {
                            string target =
                                dependentOperation.parameters.target;

                            if (
                                target.find(name) !=
                                string::npos
                            )
                            {
                                dependentOperation.parameters.target =
                                    resolvedPath;
                            }
                        }

                        // ------------------------------------------------
                        // Application launch target can refer to the
                        // resolved project.
                        // ------------------------------------------------

                        if (
                            dependentOperation.name ==
                            "application.launch"
                        )
                        {
                            string target =
                                dependentOperation.parameters.target;

                            if (
                                target.find(name) !=
                                string::npos
                            )
                            {
                                dependentOperation.parameters.target =
                                    resolvedPath;
                            }
                        }
                    }

                    return true;
                }
            }

            // ------------------------------------------------
            // Resource not found here.
            // Continue through this step's dependencies.
            // ------------------------------------------------

            for (
                const string& parentDependency
                : previousStep.dependencies
            )
            {
                pendingDependencies.push_back(
                    parentDependency
                );
            }
        }
    }

    // --------------------------------------------------------
    // FALLBACK: RESOLVE EXPLICITLY NAMED PREVIOUS RESOURCE
    // --------------------------------------------------------
    //
    // The current step may explicitly mention a resource such as:
    //
    //     "inside the NovaJava project directory"
    //     "open the NovaJava project"
    //
    // even when the LLM did not produce a dependency edge.
    // --------------------------------------------------------

    for (const MultiStepAction& previousStep : plan.steps)
    {
        if (previousStep.id == action.id)
        {
            break;
        }

        for (
            const PrimitiveOperation& operation
            : previousStep.operations
        )
        {
            if (
                operation.name ==
                "filesystem.create"
                &&
                operation.resolved
            )
            {
                const string& name =
                    operation.parameters.name;

                const string& location =
                    operation.parameters.location;

                if (
                    name.empty() ||
                    location.empty()
                )
                {
                    continue;
                }

                const bool explicitlyNamed =
                    action.description.find(name) != string::npos;

                if (!explicitlyNamed)
                {
                    size_t availableFolders = 0;
                    for (const MultiStepAction& candidateStep : plan.steps)
                    {
                        if (candidateStep.id == action.id)
                        {
                            break;
                        }

                        for (const PrimitiveOperation& candidateOperation :
                             candidateStep.operations)
                        {
                            if (candidateOperation.name == "filesystem.create" &&
                                candidateOperation.resolved &&
                                !candidateOperation.parameters.name.empty() &&
                                !candidateOperation.parameters.location.empty())
                            {
                                ++availableFolders;
                            }
                        }
                    }

                    if (!containsContextReference(action.description) ||
                        availableFolders != 1)
                    {
                        continue;
                    }
                }

                string resolvedPath;
                if (!createdResourcePath(operation, resolvedPath))
                {
                    continue;
                }

                ContextReference reference
                {
                    ContextReferenceType::NAMED_RESOURCE,
                    name,
                    previousStep.id,
                    true,
                    resolvedPath
                };

                action.contextReferences.push_back(
                    reference
                );

                // ------------------------------------------------
                // Apply the resolved resource.
                // ------------------------------------------------

                for (
                    PrimitiveOperation& dependentOperation
                    : action.operations
                )
                {
                    if (
                        dependentOperation.name ==
                        "filesystem.create_file"
                    )
                    {
                        dependentOperation.parameters.location =
                            resolvedPath;
                    }

                    if (
                        dependentOperation.name ==
                        "text.write"
                    )
                    {
                        string target =
                            dependentOperation.parameters.target;

                        if (
                            target.find(name) !=
                            string::npos
                        )
                        {
                            dependentOperation.parameters.target =
                                resolvedPath;
                        }
                    }

                    if (
                        dependentOperation.name ==
                        "application.launch"
                    )
                    {
                        string target =
                            dependentOperation.parameters.target;

                        if (
                            target.find(name) !=
                            string::npos
                        )
                        {
                            dependentOperation.parameters.target =
                                resolvedPath;
                        }
                    }
                }

                return true;
            }
        }
    }
    return false;
}
