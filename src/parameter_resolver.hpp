#pragma once

#include "action.hpp"
#include "action_parameters.hpp"
#include "capability.hpp"
#include "primitive_operation.hpp"
#include "content_resolver.hpp"

#include <string>

using namespace std;

enum class ParameterResolutionStatus
{
    RESOLVED,
    CLARIFICATION_REQUIRED,
    FAILED
};

struct ParameterResolverResult
{
    bool success;

    ParameterResolutionStatus status;

    ActionParameters parameters;

    string message;

    string clarificationQuestion;
};

class ParameterResolver
{
public:

    ParameterResolverResult resolve(
        const string& input,
        const Action& action,
        const Capability& capability
    );

    ParameterResolverResult resolve(
        const string& stepDescription,
        const PrimitiveOperation& operation,
        const Capability& capability
    );

    bool resolveIntoOperation(
        const string& stepDescription,
        PrimitiveOperation& operation,
        const Capability& capability
    );

    PrimitiveParameters toPrimitiveParameters(
        const ActionParameters& parameters
    );

private:

    ContentResolver contentResolver;
};