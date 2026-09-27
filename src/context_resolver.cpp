#include "context_resolver.hpp"
#include <iostream>

ContextResolutionResult ContextResolver::resolve(
    const MultiStepAction& action,
    const MultiStepPlan& plan
)
{
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
        text.find("project directory in ") != string::npos ||
        text.find(" project in ") != string::npos;
}

bool ContextResolver::resolvePreviousStepContext(
    MultiStepAction& action,
    const MultiStepPlan& plan
)
{
    if (!containsContextReference(action.description))
    {
        return true;
    }
    cout << "\n[CONTEXT DEBUG]" << endl;

    cout << "Action: "
        << action.id
        << endl;

    cout << "Description: "
        << action.description
        << endl;

    cout << "Dependencies: ";

    for (
        const string& dependency
        : action.dependencies
    )
    {
        cout << dependency << " ";
    }

    cout << endl;
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
            cout << "Checking dependency: "
                << dependencyId
                << endl;

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

                    string resolvedPath =
                        location + "/" + name;

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

                // The resource must be explicitly named
                // in the current action description.
                if (
                    action.description.find(name) ==
                    string::npos
                )
                {
                    continue;
                }

                string resolvedPath =
                    location + "/" + name;

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
    cout << "No contextual resource resolved."
     << endl;

    return false;
}