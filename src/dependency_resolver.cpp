#include "dependency_resolver.hpp"

#include <unordered_map>
#include <unordered_set>
#include <queue>

DependencyResolutionResult DependencyResolver::resolve(
    const MultiStepPlan& plan
)
{
    DependencyResolutionResult result;

    result.valid = false;
    result.executionOrder.clear();
    result.message = "";

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

    unordered_map<string, int> inDegree;

    unordered_map<string, vector<string>> dependents;

    for (const auto& step : plan.steps)
    {
        inDegree[step.id] = 0;
    }

    for (const auto& step : plan.steps)
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

    for (const auto& step : plan.steps)
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

    if (result.executionOrder.size() != plan.steps.size())
    {
        result.executionOrder.clear();

        result.message =
            "Dependency cycle detected in multi-step plan.";

        return result;
    }

    result.valid = true;

    result.message =
        "Dependencies resolved successfully.";

    return result;
}