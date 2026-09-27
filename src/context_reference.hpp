#pragma once

#include <string>
using namespace std;

enum class ContextReferenceType
{
    PREVIOUS_STEP_OUTPUT,
    PREVIOUS_STEP_TARGET,
    PREVIOUS_STEP_LOCATION,
    NAMED_RESOURCE
};

struct ContextReference
{
    ContextReferenceType type;

    string reference;
    string sourceStepId;

    bool resolved;
    string resolvedValue;
};