#pragma once

#include <string>

using namespace std;

enum class RequestType
{
    CONVERSATION,
    SINGLE_ACTION,
    MULTI_STEP_GOAL,
    EXISTING_WORKFLOW,
    CLARIFICATION_REQUIRED,
    UNSUPPORTED
};

struct ClassificationResult
{
    RequestType type;
    bool success;
    string message;
};