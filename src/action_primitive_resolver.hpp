#pragma once

#include "multistep_plan.hpp"

#include <string>

using namespace std;

enum class ActionPrimitiveResolutionStatus
{
    RESOLVED,
    CLARIFICATION_REQUIRED,
    FAILED
};

struct ActionPrimitiveResolutionResult
{
    ActionPrimitiveResolutionStatus status;

    MultiStepAction action;

    string message;
    string clarificationQuestion;
};

class ActionPrimitiveResolver
{
public:
    ActionPrimitiveResolutionResult resolve(
        const MultiStepAction& action
    );
};