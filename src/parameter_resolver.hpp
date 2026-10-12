#pragma once

#include "action.hpp"
#include "action_parameters.hpp"
#include "capability.hpp"
#include "primitive_operation.hpp"
#include "primitive_registry.hpp"
#include "llm_client.hpp"

#include <functional>
#include <string>
#include <utility>
#include <vector>

using namespace std;

enum class ParameterResolutionStatus
{
    RESOLVED,
    AMBIGUOUS,
    MISSING_REQUIRED_PARAMETER,
    INVALID_RESPONSE,
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
    using LLMGenerator = function<LLMResponse(
        const string&,
        const string&
    )>;

    ParameterResolver() = default;

    explicit ParameterResolver(LLMGenerator llmGenerator);

    // Branch 2
    ParameterResolverResult resolve(
        const string& input,
        const Action& action,
        const Capability& capability
    );

    // Branch 3
    ParameterResolverResult resolve(
        const string& stepDescription,
        const PrimitiveOperation& operation,
        const Capability& capability,
        const vector<pair<string, string>>& availableContext = {}
    );

    bool resolveIntoOperation(
        const string& stepDescription,
        PrimitiveOperation& operation,
        const Capability& capability,
        const vector<pair<string, string>>& availableContext = {}
    );

    bool toPrimitiveParameters(
        const ActionParameters& parameters,
        PrimitiveParameters& primitiveParameters,
        string& errorMessage
    );

private:

    ParameterResolverResult resolveSemanticParameters(
        const string& description,
        const PrimitiveOperation& operation,
        const PrimitiveDefinition& definition,
        const vector<pair<string, string>>& availableContext
    );

    bool validateParameters(
        const ActionParameters& parameters,
        const PrimitiveParameterSchema& schema,
        string& errorMessage
    );

    ParameterValue makeLiteral(
        const string& value
    );


    LLMGenerator llmGenerator;
};