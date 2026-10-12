#include "dependency_resolver.hpp"

#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <algorithm>
#include <cctype>

namespace
{
    string toLower(string text)
    {
        transform(
            text.begin(),
            text.end(),
            text.begin(),
            [](unsigned char character)
            {
                return static_cast<char>(
                    tolower(character)
                );
            }
        );

        return text;
    }

    bool contains(
        const string& text,
        const string& value
    )
    {
        return text.find(value) != string::npos;
    }

    bool referencesStepResource(
        const MultiStepAction& currentStep,
        const MultiStepAction& previousStep
    )
    {
        const string current =
            toLower(currentStep.description);

        const string resource =
            toLower(previousStep.description);

        /*
         * Resource names are inferred from the already-resolved
         * primitive parameters when available.
         */

        for (const auto& operation :
             previousStep.operations)
        {
            const string name =
                toLower(operation.parameters.name);

            if (
                !name.empty()
                &&
                contains(current, name)
            )
            {
                return true;
            }
        }

        /*
         * Explicit contextual language can also indicate
         * dependency on the previous step's resource.
         */

        if (
            contains(current, "inside the")
            ||
            contains(current, "inside ")
            ||
            contains(current, "within the")
            ||
            contains(current, "in the")
            ||
            contains(current, "open the")
            ||
            contains(current, "that folder")
            ||
            contains(current, "that directory")
            ||
            contains(current, "that file")
        )
        {
            return true;
        }

        return false;
    }
}

DependencyResolutionResult DependencyResolver::resolve(
    const MultiStepPlan& plan
)
{
    DependencyResolutionResult result;

    result.valid = false;
    result.executionOrder.clear();
    result.message = "";

    MultiStepPlan resolvedPlan = plan;

    if (!plan.valid)
    {
        result.message =
            "Cannot resolve dependencies from an invalid multi-step plan.";

        return result;
    }

    if (plan.steps.empty())
    {
        result.message =
            "Multi-step plan contains no steps.";

        return result;
    }

    unordered_map<string, const MultiStepAction*> stepMap;

    for (const auto& step : plan.steps)
    {
        if (step.id.empty())
        {
            result.message =
                "A step has an empty ID.";

            return result;
        }

        if (stepMap.contains(step.id))
        {
            result.message =
                "Duplicate step ID: " + step.id;

            return result;
        }

        stepMap[step.id] = &step;
    }

    // --------------------------------------------------
    // INFER MISSING RESOURCE DEPENDENCIES
    // --------------------------------------------------

    for (size_t currentIndex = 0;
        currentIndex < resolvedPlan.steps.size();
        ++currentIndex)
    {
        auto& currentStep =
            resolvedPlan.steps[currentIndex];

        for (size_t previousIndex = 0;
            previousIndex < currentIndex;
            ++previousIndex)
        {
            const auto& previousStep =
                resolvedPlan.steps[previousIndex];

            if (
                referencesStepResource(
                    currentStep,
                    previousStep
                )
            )
            {
                if (
                    find(
                        currentStep.dependencies.begin(),
                        currentStep.dependencies.end(),
                        previousStep.id
                    )
                    ==
                    currentStep.dependencies.end()
                )
                {
                    currentStep.dependencies.push_back(
                        previousStep.id
                    );
                }
            }
        }
    }

    unordered_map<string, int> inDegree;

    unordered_map<string, vector<string>> dependents;

    for (const auto& step : resolvedPlan.steps)
    {
        inDegree[step.id] = 0;
    }

    for (const auto& step : resolvedPlan.steps)
    {
        for (const auto& dependency : step.dependencies)
        {
            if (!stepMap.contains(dependency))
            {
                result.message =
                    "Step '" +
                    step.id +
                    "' depends on unknown step '" +
                    dependency +
                    "'.";

                return result;
            }

            if (dependency == step.id)
            {
                result.message =
                    "Step '" +
                    step.id +
                    "' cannot depend on itself.";

                return result;
            }

            dependents[dependency].push_back(step.id);

            inDegree[step.id]++;
        }
    }

    queue<string> readySteps;

    for (const auto& step : resolvedPlan.steps)
    {
        if (inDegree[step.id] == 0)
        {
            readySteps.push(step.id);
        }
    }

    while (!readySteps.empty())
    {
        string current =
            readySteps.front();

        readySteps.pop();

        result.executionOrder.push_back(current);

        for (const auto& dependent :
             dependents[current])
        {
            inDegree[dependent]--;

            if (inDegree[dependent] == 0)
            {
                readySteps.push(dependent);
            }
        }
    }

    if (
        result.executionOrder.size() !=
        resolvedPlan.steps.size()
    )
    {
        result.executionOrder.clear();

        result.message =
            "Dependency cycle detected in multi-step plan.";

        return result;
    }

    result.plan =
        resolvedPlan;

    result.valid = true;

    result.message =
        "Dependencies resolved successfully.";

    return result;
}