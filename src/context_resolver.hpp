#pragma once

#include "multistep_plan.hpp"

#include <string>
#include <unordered_map>
using namespace std;

enum class ContextResolutionStatus
{
    RESOLVED,
    CLARIFICATION_REQUIRED,
    FAILED
};

struct ContextResolutionResult
{
    ContextResolutionStatus status;

    MultiStepAction action;

    unordered_map<string, string> resolvedContext;

    string message;
    string clarificationQuestion;
};

class ContextResolver
{
public:

    ContextResolutionResult resolve(
        const MultiStepAction& action,
        const MultiStepPlan& plan
    );

    bool containsContextReference(
        const string& description
    );
    bool resolvePreviousStepContext(
        MultiStepAction& action,
        const MultiStepPlan& plan
    );
};