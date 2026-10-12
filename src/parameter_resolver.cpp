#include "parameter_resolver.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <initializer_list>
#include <sstream>
#include <utility>

using namespace std;
using json = nlohmann::json;

namespace
{
    string stripWrappingQuotes(const string& value)
    {
        if (value.size() >= 2)
        {
            const char first = value.front();
            const char last = value.back();

            if ((first == '"' && last == '"') ||
                (first == '\'' && last == '\''))
            {
                return value.substr(1, value.size() - 2);
            }
        }

        return value;
    }

    bool hasExactlyKeys(
        const json& value,
        initializer_list<const char*> keys
    )
    {
        if (!value.is_object() || value.size() != keys.size())
        {
            return false;
        }

        for (const char* key : keys)
        {
            if (!value.contains(key))
            {
                return false;
            }
        }

        return true;
    }

    bool isMeaningful(const string& value)
    {
        return any_of(
            value.begin(),
            value.end(),
            [](unsigned char character)
            {
                return !isspace(character);
            }
        );
    }

    const char* requirementName(ParameterRequirement requirement)
    {
        switch (requirement)
        {
            case ParameterRequirement::NOT_APPLICABLE:
                return "not_applicable";
            case ParameterRequirement::REQUIRED:
                return "required";
            case ParameterRequirement::OPTIONAL:
                return "optional";
        }

        return "invalid";
    }

    bool parseParameterValue(
        const json& value,
        ParameterValue& parameter
    )
    {
        if (!value.is_object() || !value.contains("type") ||
            !value["type"].is_string())
        {
            return false;
        }

        const string type = value["type"].get<string>();

        if (type == "none")
        {
            if (!hasExactlyKeys(value, {"type"}))
            {
                return false;
            }

            parameter = ParameterValue{};
            return true;
        }

        if (type == "literal")
        {
            if (!hasExactlyKeys(value, {"type", "value"}) ||
                !value["value"].is_string())
            {
                return false;
            }

            parameter.type = ParameterValueType::LITERAL;
            parameter.value = value["value"].get<string>();
            parameter.reference = ContextParameterReference{};
            return true;
        }

        if (type == "context_reference")
        {
            if (!hasExactlyKeys(value, {"type", "step_id", "output"}) ||
                !value["step_id"].is_string() ||
                !value["output"].is_string())
            {
                return false;
            }

            parameter.type = ParameterValueType::CONTEXT_REFERENCE;
            parameter.value.clear();
            parameter.reference.stepId =
                value["step_id"].get<string>();
            parameter.reference.output =
                value["output"].get<string>();
            return true;
        }

        return false;
    }

    bool parameterMatchesRequirement(
        const ParameterValue& parameter,
        ParameterRequirement requirement,
        const string& name,
        string& errorMessage
    )
    {
        if (parameter.type == ParameterValueType::NONE)
        {
            if (requirement == ParameterRequirement::REQUIRED)
            {
                errorMessage = "Required parameter is missing: " + name;
                return false;
            }

            return true;
        }

        if (requirement == ParameterRequirement::NOT_APPLICABLE)
        {
            errorMessage = "Parameter is not applicable: " + name;
            return false;
        }

        if (parameter.type == ParameterValueType::LITERAL)
        {
            if (!isMeaningful(parameter.value))
            {
                errorMessage = "Literal parameter is empty: " + name;
                return false;
            }

            return true;
        }

        if (parameter.type == ParameterValueType::CONTEXT_REFERENCE)
        {
            if (!isMeaningful(parameter.reference.stepId) ||
                !isMeaningful(parameter.reference.output))
            {
                errorMessage = "Context reference is incomplete: " + name;
                return false;
            }

            return true;
        }

        errorMessage = "Unknown parameter value type: " + name;
        return false;
    }

    string makeSystemPrompt(
        const PrimitiveDefinition& definition,
        const PrimitiveParameterSchema& schema
    )
    {
        ostringstream prompt;
        prompt << R"(You are Nova's semantic parameter resolver.
Interpret the step description for the specified primitive. Do not execute anything.
Return only strict JSON matching one of the response schemas below, with no Markdown or extra keys.

Resolved response:
{"status":"resolved","parameters":{"name":{"type":"none"},"location":{"type":"none"},"application":{"type":"none"},"target":{"type":"none"},"content":{"type":"none"}}}

Each parameter must be exactly one of:
{"type":"none"}
{"type":"literal","value":"..."}
{"type":"context_reference","step_id":"step_1","output":"path"}

Ambiguous response:
{"status":"ambiguous","question":"..."}

Missing required response:
{"status":"missing_required_parameter","question":"..."}

Rules:
1. Preserve user-provided names exactly.
2. Do not invent filenames, folder names, paths, or content.
3. Do not infer file content merely from an extension.
4. Do not convert contextual references into guessed literal paths.
5. Use context_reference when a value depends on a previous step.
6. Return only the requested JSON schema.
7. Never invent a missing required parameter; report ambiguity or missing-required information instead.
8. Use type none for parameters that were not supplied or are not applicable.
9. A request to add a code artifact such as a basic class or program explicitly requests its content; return the minimal requested code as a literal.
10. A request only to create a file by name, even with a source-code extension, leaves content as none.
11. Escape every quote, backslash, and line break inside JSON string values; never emit raw line breaks inside a JSON string.)";

        prompt << "\n\nPrimitive type: "
               << static_cast<int>(definition.type)
               << "\nParameter requirements:\n"
               << "name: " << requirementName(schema.name) << "\n"
               << "location: " << requirementName(schema.location) << "\n"
               << "application: " << requirementName(schema.application) << "\n"
               << "target: " << requirementName(schema.target) << "\n"
               << "content: " << requirementName(schema.content);
        return prompt.str();
    }

    string makeUserPrompt(
        const string& description,
        const PrimitiveOperation& operation,
        const vector<pair<string, string>>& availableContext
    )
    {
        ostringstream prompt;
        prompt << "Primitive: " << operation.name
            << "\nStep description: " << description
            << "\nResolve only information supported by this description."
            << "\nValid context outputs are path, name, location, application, target, and content."
            << "\nUse a context_reference only with one of the exact available step IDs below."
            << "\nWhen this step refers to a resource created by an available dependency, use that step's path output for location or target; never return the resource name as a guessed path.";

        for (const auto& [stepId, stepDescription] : availableContext)
        {
            prompt << "\nAvailable step ID: " << stepId
                << "\nAvailable step description: " << stepDescription;
        }

        if (availableContext.empty())
        {
            prompt << "\nNo previous-step context is available.";
        }

        return prompt.str();
    }
}

ParameterResolver::ParameterResolver(LLMGenerator generator)
    : llmGenerator(move(generator))
{
}

ParameterValue ParameterResolver::makeLiteral(const string& value)
{
    return ParameterValue{
        ParameterValueType::LITERAL,
        value,
        ContextParameterReference{}
    };
}

bool ParameterResolver::validateParameters(
    const ActionParameters& parameters,
    const PrimitiveParameterSchema& schema,
    string& errorMessage
)
{
    return
        parameterMatchesRequirement(parameters.name, schema.name, "name", errorMessage) &&
        parameterMatchesRequirement(parameters.location, schema.location, "location", errorMessage) &&
        parameterMatchesRequirement(parameters.application, schema.application, "application", errorMessage) &&
        parameterMatchesRequirement(parameters.target, schema.target, "target", errorMessage) &&
        parameterMatchesRequirement(parameters.content, schema.content, "content", errorMessage);
}

ParameterResolverResult ParameterResolver::resolve(
    const string& input,
    const Action& action,
    const Capability& capability
)
{
    ParameterResolverResult result{};
    result.status = ParameterResolutionStatus::FAILED;

    if (input.empty())
    {
        result.message = "Cannot resolve parameters from empty input.";
        return result;
    }

    if (!capability.supported)
    {
        result.message =
            "Cannot resolve parameters for unsupported capability.";
        return result;
    }

    if (action.name == "create_folder")
    {
        const size_t marker = input.find("called ");
        if (marker == string::npos)
        {
            result.status = ParameterResolutionStatus::MISSING_REQUIRED_PARAMETER;
            result.message = "Folder name is required.";
            result.clarificationQuestion =
                "What should the project folder be named?";
            return result;
        }

        const size_t nameStart = marker + string("called ").size();
        size_t nameEnd = input.find(" on ", nameStart);
        if (nameEnd == string::npos)
        {
            nameEnd = input.size();
        }

        const string name = stripWrappingQuotes(
            input.substr(nameStart, nameEnd - nameStart));
        if (!isMeaningful(name))
        {
            result.status = ParameterResolutionStatus::MISSING_REQUIRED_PARAMETER;
            result.message = "Folder name is required.";
            result.clarificationQuestion =
                "What should the project folder be named?";
            return result;
        }

        result.parameters.name = makeLiteral(name);
        result.parameters.location = makeLiteral("desktop");
        result.success = true;
        result.status = ParameterResolutionStatus::RESOLVED;
        result.message = "Folder parameters resolved successfully.";
        return result;
    }

    if (action.name == "open_app" || action.name == "close_app")
    {
        string application;
        const string prefixes[] = {"open ", "launch ", "close "};
        for (const string& prefix : prefixes)
        {
            if (input.rfind(prefix, 0) == 0)
            {
                application = stripWrappingQuotes(input.substr(prefix.size()));
                break;
            }
        }

        if (!isMeaningful(application))
        {
            result.status = ParameterResolutionStatus::MISSING_REQUIRED_PARAMETER;
            result.message = "Application parameter is required.";
            result.clarificationQuestion =
                "Which application should Nova use?";
            return result;
        }

        result.parameters.application = makeLiteral(application);
        result.success = true;
        result.status = ParameterResolutionStatus::RESOLVED;
        result.message = "Application parameter resolved successfully.";
        return result;
    }

    result.message = "No parameter resolution rule exists for requested action.";
    return result;
}

ParameterResolverResult ParameterResolver::resolveSemanticParameters(
    const string& description,
    const PrimitiveOperation& operation,
    const PrimitiveDefinition& definition,
    const vector<pair<string, string>>& availableContext
)
{
    ParameterResolverResult result{};
    result.status = ParameterResolutionStatus::FAILED;

    const PrimitiveParameterSchema& schema = *definition.parameters;
    const string systemPrompt = makeSystemPrompt(definition, schema);
    const string userPrompt = makeUserPrompt(
        description,
        operation,
        availableContext);
    LLMResponse response;

    if (llmGenerator)
    {
        response = llmGenerator(systemPrompt, userPrompt);
    }
    else
    {
        LLMClient client;
        response = client.generate(systemPrompt, userPrompt);
    }

    if (!response.success || response.content.empty())
    {
        result.message = "Parameter resolution LLM request failed: " +
            response.message;
        return result;
    }

    try
    {
        const json parsed = json::parse(response.content);
        if (!parsed.is_object() || !parsed.contains("status") ||
            !parsed["status"].is_string())
        {
            result.status = ParameterResolutionStatus::INVALID_RESPONSE;
            result.message = "Parameter resolver response has no valid status.";
            return result;
        }

        const string status = parsed["status"].get<string>();
        if (status == "ambiguous" ||
            status == "missing_required_parameter")
        {
            if (!hasExactlyKeys(parsed, {"status", "question"}) ||
                !parsed["question"].is_string() ||
                !isMeaningful(parsed["question"].get<string>()))
            {
                result.status = ParameterResolutionStatus::INVALID_RESPONSE;
                result.message =
                    "Parameter resolver clarification response is malformed.";
                return result;
            }

            result.status = status == "ambiguous"
                ? ParameterResolutionStatus::AMBIGUOUS
                : ParameterResolutionStatus::MISSING_REQUIRED_PARAMETER;
            result.message = status == "ambiguous"
                ? "Parameter resolution is ambiguous."
                : "A required parameter is missing.";
            result.clarificationQuestion =
                parsed["question"].get<string>();
            return result;
        }

        if (status != "resolved" ||
            !hasExactlyKeys(parsed, {"status", "parameters"}) ||
            !hasExactlyKeys(
                parsed["parameters"],
                {"name", "location", "application", "target", "content"}))
        {
            result.status = ParameterResolutionStatus::INVALID_RESPONSE;
            result.message = "Parameter resolver returned an invalid JSON schema.";
            return result;
        }

        const json& parameters = parsed["parameters"];
        const bool parsedValues =
            parseParameterValue(parameters["name"], result.parameters.name) &&
            parseParameterValue(parameters["location"], result.parameters.location) &&
            parseParameterValue(parameters["application"], result.parameters.application) &&
            parseParameterValue(parameters["target"], result.parameters.target) &&
            parseParameterValue(parameters["content"], result.parameters.content);

        if (!parsedValues)
        {
            result.status = ParameterResolutionStatus::INVALID_RESPONSE;
            result.message = "Parameter resolver returned an invalid parameter value.";
            return result;
        }

        string validationError;
        if (!validateParameters(result.parameters, schema, validationError))
        {
            result.status = ParameterResolutionStatus::INVALID_RESPONSE;
            result.message = "Parameter schema validation failed: " +
                validationError;
            return result;
        }

        result.success = true;
        result.status = ParameterResolutionStatus::RESOLVED;
        result.message = "Semantic parameters resolved successfully.";
        return result;
    }
    catch (const exception& error)
    {
        result.status = ParameterResolutionStatus::INVALID_RESPONSE;
        result.message = string("Invalid parameter resolver JSON: ") +
            error.what();
        return result;
    }
}

ParameterResolverResult ParameterResolver::resolve(
    const string& stepDescription,
    const PrimitiveOperation& operation,
    const Capability& capability,
    const vector<pair<string, string>>& availableContext
)
{
    ParameterResolverResult result{};
    result.status = ParameterResolutionStatus::FAILED;

    if (stepDescription.empty())
    {
        result.message =
            "Cannot resolve parameters from empty step description.";
        return result;
    }

    if (!operation.resolved)
    {
        result.message =
            "Cannot resolve parameters for unresolved primitive.";
        return result;
    }

    if (!capability.supported)
    {
        result.message =
            "Cannot resolve parameters for unsupported capability.";
        return result;
    }

    PrimitiveDefinition definition{};
    if (!PrimitiveRegistry::resolveDefinition(operation.name, definition))
    {
        result.message = "Unsupported primitive: " + operation.name;
        return result;
    }

    if (operation.type != definition.type)
    {
        result.status = ParameterResolutionStatus::INVALID_RESPONSE;
        result.message = "Primitive name and operation type do not match.";
        return result;
    }

    if (!definition.parameters.has_value())
    {
        result.message =
            "Primitive parameter schema is not defined: " + operation.name;
        return result;
    }

    return resolveSemanticParameters(
        stepDescription,
        operation,
        definition,
        availableContext);
}

bool ParameterResolver::toPrimitiveParameters(
    const ActionParameters& parameters,
    PrimitiveParameters& primitiveParameters,
    string& errorMessage
)
{
    const auto convert = [&errorMessage](
        const ParameterValue& value,
        string& output,
        const string& name)
    {
        if (value.type == ParameterValueType::NONE)
        {
            output.clear();
            return true;
        }

        if (value.type != ParameterValueType::LITERAL ||
            !isMeaningful(value.value))
        {
            errorMessage =
                "Parameter must be resolved to a meaningful literal before execution: " +
                name;
            return false;
        }

        output = value.value;
        return true;
    };

    return
        convert(parameters.name, primitiveParameters.name, "name") &&
        convert(parameters.location, primitiveParameters.location, "location") &&
        convert(parameters.application, primitiveParameters.application, "application") &&
        convert(parameters.target, primitiveParameters.target, "target") &&
        convert(parameters.content, primitiveParameters.content, "content");
}

bool ParameterResolver::resolveIntoOperation(
    const string& stepDescription,
    PrimitiveOperation& operation,
    const Capability& capability,
    const vector<pair<string, string>>& availableContext
)
{
    const ParameterResolverResult result = resolve(
        stepDescription,
        operation,
        capability,
        availableContext);

    if (!result.success)
    {
        return false;
    }

    PrimitiveParameters primitiveParameters;
    string errorMessage;
    if (!toPrimitiveParameters(
            result.parameters,
            primitiveParameters,
            errorMessage))
    {
        return false;
    }

    operation.parameters = primitiveParameters;
    return true;
}
