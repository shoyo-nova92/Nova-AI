#include "primitive_mapper.hpp"
#include "utf8_text.hpp"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <exception>
#include <set>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

#include <nlohmann/json.hpp>

#include "primitive_registry.hpp"

using nlohmann::json;

namespace {

string requirementToString(ParameterRequirement value) {
    switch (value) {
        case ParameterRequirement::REQUIRED: return "required";
        case ParameterRequirement::OPTIONAL: return "optional";
        case ParameterRequirement::NOT_APPLICABLE: return "not_applicable";
    }
    return "not_applicable";
}

string typeToString(PrimitiveParameterType value) {
    switch (value) {
        case PrimitiveParameterType::STRING: return "string";
        case PrimitiveParameterType::TEXT: return "text";
        case PrimitiveParameterType::FILE_PATH: return "file_path";
        case PrimitiveParameterType::DIRECTORY_PATH: return "directory_path";
        case PrimitiveParameterType::APPLICATION: return "application";
        case PrimitiveParameterType::URL: return "url";
        case PrimitiveParameterType::KEY: return "key";
        case PrimitiveParameterType::MODIFIER_LIST: return "modifier_list";
        case PrimitiveParameterType::ENUM: return "enum";
        case PrimitiveParameterType::INTEGER: return "integer";
        case PrimitiveParameterType::NUMBER: return "number";
        case PrimitiveParameterType::BOOLEAN: return "boolean";
    }
    return "string";
}

string trim(const string& value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == string::npos) return {};
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

MultiStepPlan failedPlan(const MultiStepPlan& source, const string& message) {
    MultiStepPlan result = source;
    result.valid = false;
    result.steps.clear();
    result.message = message;
    return result;
}

json catalogToJson() {
    json catalog = json::array();
    for (const auto& item : PrimitiveRegistry::getCatalog()) {
        const string& name = item.first;
        const PrimitiveDefinition& definition = item.second;
        json entry = {
            {"name", name},
            {"description", definition.description},
            {"capability", definition.capability},
            {"parameters", json::array()},
            {"outputs", json::array()}
        };

        if (definition.parameters.has_value()) {
            for (const auto& field : definition.parameters->fields) {
                json fieldJson = {
                    {"name", field.name},
                    {"type", typeToString(field.type)},
                    {"requirement", requirementToString(field.requirement)},
                    {"description", field.description},
                    {"allowed_values", field.allowedValues},
                    {"has_minimum", field.hasMinimum},
                    {"has_maximum", field.hasMaximum}
                };
                if (field.hasMinimum) fieldJson["minimum"] = field.minimum;
                if (field.hasMaximum) fieldJson["maximum"] = field.maximum;
                entry["parameters"].push_back(std::move(fieldJson));
            }
        }
        for (const auto& output : definition.outputs) {
            entry["outputs"].push_back({
                {"name", output.name},
                {"type", output.type},
                {"description", output.description}
            });
        }
        catalog.push_back(std::move(entry));
    }
    return catalog;
}

json sourcePlanToJson(const MultiStepPlan& plan) {
    json result = {{"goal", plan.goal}, {"steps", json::array()}};
    for (const auto& step : plan.steps) {
        result["steps"].push_back({
            {"id", step.id},
            {"description", step.description},
            {"depends_on", step.dependencies}
        });
    }
    return result;
}

bool assignParameter(PrimitiveParameters& output, const string& name,
                     const string& value, string& error) {
    // Keep this explicit: if the registry grows, unsupported storage fields fail loudly.
    if (name == "name") output.name = value;
    else if (name == "location") output.location = value;
    else if (name == "application") output.application = value;
    else if (name == "target") output.target = value;
    else if (name == "content") output.content = value;
    else if (name == "source") output.source = value;
    else if (name == "destination") output.destination = value;
    else if (name == "newName") output.newName = value;
    else if (name == "recursive") output.recursive = value;
    else if (name == "overwrite") output.overwrite = value;
    else if (name == "query") output.query = value;
    else if (name == "max_results") output.maxResults = value;
    else if (name == "entry_type") output.entryType = value;
    else if (name == "extension") output.extension = value;
    else if (name == "modified_within_days") output.modifiedWithinDays = value;
    else if (name == "key") output.key = value;
    else if (name == "modifiers") output.modifiers = value;
    else if (name == "repeatCount") output.repeatCount = value;
    else if (name == "x") output.x = value;
    else if (name == "y") output.y = value;
    else if (name == "button") output.button = value;
    else if (name == "clickCount") output.clickCount = value;
    else if (name == "url") output.url = value;
    else if (name == "ready_condition") output.readyCondition = value;
    else if (name == "selector") output.selector = value;
    else if (name == "timeout_ms") output.timeoutMs = value;
    else if (name == "role") output.role = value;
    else if (name == "text") output.text = value;
    else if (name == "element_id") output.elementId = value;
    else if (name == "submit") output.submit = value;
    else if (name == "process_id") output.processId = value;
    else if (name == "window_handle") output.windowHandle = value;
    else {
        error = "Registry parameter '" + name + "' has no PrimitiveParameters storage field.";
        return false;
    }
    return true;
}

bool validHttpUrlValue(const string& value) {
    if (value.empty() || value.size() > 8192) return false;
    for (unsigned char ch : value) if (ch <= 0x20 || ch == 0x7f) return false;
    const size_t schemeEnd = value.find("://");
    if (schemeEnd == string::npos) return false;
    string scheme = value.substr(0, schemeEnd);
    transform(scheme.begin(), scheme.end(), scheme.begin(),
        [](unsigned char c) { return static_cast<char>(tolower(c)); });
    if (scheme != "http" && scheme != "https") return false;
    const size_t authorityStart = schemeEnd + 3;
    const size_t authorityEnd = value.find_first_of("/?#", authorityStart);
    const string authority = value.substr(authorityStart,
        authorityEnd == string::npos ? string::npos : authorityEnd - authorityStart);
    return !authority.empty() && authority.find('@') == string::npos;
}

bool convertAndValidateValue(const json& value,
                             const PrimitiveParameterDefinition& field,
                             string& output,
                             string& error) {
    const string label = "Parameter '" + field.name + "' ";
    double numericValue = 0.0;

    switch (field.type) {
        case PrimitiveParameterType::STRING:
        case PrimitiveParameterType::TEXT:
        case PrimitiveParameterType::FILE_PATH:
        case PrimitiveParameterType::DIRECTORY_PATH:
        case PrimitiveParameterType::APPLICATION:
        case PrimitiveParameterType::KEY:
        case PrimitiveParameterType::MODIFIER_LIST:
        case PrimitiveParameterType::ENUM:
            if (!value.is_string()) {
                error = label + "must be a JSON string.";
                return false;
            }
            output = value.get<string>();
            if (trim(output).empty()) {
                error = label + "must not be empty.";
                return false;
            }
            if (field.type == PrimitiveParameterType::ENUM &&
                !field.allowedValues.empty() &&
                std::find(field.allowedValues.begin(), field.allowedValues.end(), output) == field.allowedValues.end()) {
                error = label + "must be one of the allowed values.";
                return false;
            }
            return true;

        case PrimitiveParameterType::URL:
            if (!value.is_string()) {
                error = label + "must be a JSON string.";
                return false;
            }
            output = value.get<string>();
            if (!validHttpUrlValue(output)) {
                error = label + "must be an absolute HTTP or HTTPS URL without embedded credentials or whitespace.";
                return false;
            }
            return true;

        case PrimitiveParameterType::INTEGER:
            if (!value.is_number_integer() && !value.is_number_unsigned()) {
                error = label + "must be an integer.";
                return false;
            }
            numericValue = value.get<double>();
            output = value.dump();
            break;

        case PrimitiveParameterType::NUMBER:
            if (!value.is_number()) {
                error = label + "must be a number.";
                return false;
            }
            numericValue = value.get<double>();
            if (!std::isfinite(numericValue)) {
                error = label + "must be finite.";
                return false;
            }
            output = value.dump();
            break;

        case PrimitiveParameterType::BOOLEAN:
            if (!value.is_boolean()) {
                error = label + "must be true or false.";
                return false;
            }
            output = value.get<bool>() ? "true" : "false";
            return true;
    }

    if (field.hasMinimum && numericValue < field.minimum) {
        error = label + "is below its minimum of " + to_string(field.minimum) + ".";
        return false;
    }
    if (field.hasMaximum && numericValue > field.maximum) {
        error = label + "exceeds its maximum of " + to_string(field.maximum) + ".";
        return false;
    }
    return true;
}

bool parseOperations(const json& stepJson, MultiStepAction& target,
                     const unordered_map<string, PrimitiveDefinition>& definitions,
                     string& error) {
    if (!stepJson.is_object() || !stepJson.contains("id") || !stepJson["id"].is_string() ||
        !stepJson.contains("operations") || !stepJson["operations"].is_array()) {
        error = "Each mapped step must contain a string 'id' and an 'operations' array.";
        return false;
    }
    if (stepJson.size() != 2) {
        error = "Mapped step may contain only 'id' and 'operations'.";
        return false;
    }
    if (stepJson["id"].get<string>() != target.id) {
        error = "Mapper changed or reordered step ID '" + target.id + "'.";
        return false;
    }
    if (stepJson["operations"].empty()) {
        error = "Step '" + target.id + "' has no mapped operations.";
        return false;
    }

    vector<PrimitiveOperation> operations;
    for (const auto& operationJson : stepJson["operations"]) {
        if (!operationJson.is_object() || !operationJson.contains("name") ||
            !operationJson["name"].is_string() || !operationJson.contains("parameters") ||
            !operationJson["parameters"].is_object()) {
            error = "Each operation must contain a string 'name' and object 'parameters'.";
            return false;
        }
        if (operationJson.size() != 2) {
            error = "Operation may contain only 'name' and 'parameters'.";
            return false;
        }

        const string primitiveName = operationJson["name"].get<string>();
        const auto definitionIt = definitions.find(primitiveName);
        if (definitionIt == definitions.end()) {
            error = "Unsupported primitive '" + primitiveName + "'.";
            return false;
        }
        const PrimitiveDefinition& definition = definitionIt->second;
        if (!definition.parameters.has_value()) {
            error = "Primitive '" + primitiveName + "' has no parameter schema; refusing unvalidated mapping.";
            return false;
        }

        const auto& fields = definition.parameters->fields;
        unordered_map<string, const PrimitiveParameterDefinition*> fieldByName;
        for (const auto& field : fields) fieldByName.emplace(field.name, &field);

        const json& paramsJson = operationJson["parameters"];
        for (auto it = paramsJson.begin(); it != paramsJson.end(); ++it) {
            if (fieldByName.find(it.key()) == fieldByName.end()) {
                error = "Primitive '" + primitiveName + "' received unknown parameter '" + it.key() + "'.";
                return false;
            }
        }

        PrimitiveOperation operation;
        operation.type = definition.type;
        operation.name = primitiveName;
        for (const auto& field : fields) {
            const auto valueIt = paramsJson.find(field.name);
            if (valueIt == paramsJson.end()) {
                if (field.requirement == ParameterRequirement::REQUIRED) {
                    error = "Primitive '" + primitiveName + "' is missing required parameter '" + field.name + "'.";
                    return false;
                }
                continue;
            }
            if (field.requirement == ParameterRequirement::NOT_APPLICABLE) {
                error = "Primitive '" + primitiveName + "' does not accept parameter '" + field.name + "'.";
                return false;
            }
            string converted;
            if (!convertAndValidateValue(*valueIt, field, converted, error)) {
                error = "Primitive '" + primitiveName + "': " + error;
                return false;
            }
            if (!assignParameter(operation.parameters, field.name, converted, error)) return false;
        }
        operation.resolved = true;
        operations.push_back(std::move(operation));
    }

    target.operations = std::move(operations);
    target.resolved = true;
    target.operation.clear();
    target.capability.clear();
    target.parameters.clear();
    if (target.operations.size() == 1) {
        target.operation = target.operations.front().name;
        target.capability = definitions.at(target.operations.front().name).capability;
    }
    return true;
}

} // namespace

PrimitiveMapper::PrimitiveMapper()
    : generator_([](const string& systemPrompt, const string& userPrompt) {
          LLMClient client;
          return client.generate(systemPrompt, userPrompt);
      }) {}

PrimitiveMapper::PrimitiveMapper(PrimitiveMapperLLM generator)
    : generator_(std::move(generator)) {}

MultiStepPlan PrimitiveMapper::mapStep(const MultiStepAction& sourceStep, const string& operationJson) {
    MultiStepPlan result{};
    result.valid = false;
    result.goal = sourceStep.description;
    result.steps = {sourceStep};
    result.steps.front().operations.clear();
    result.steps.front().resolved = false;
    try {
        json operation = json::parse(operationJson);
        if (operation.value("name", string{}) == "filesystem.create_file") {
            auto& parameters = operation.at("parameters");
            const bool blank = !parameters.contains("content") ||
                (parameters["content"].is_string() && trim(parameters["content"].get<string>()).empty());
            // An explicitly empty file may use either omission or an empty string.
            if (sourceStep.contentRequirement == "empty_allowed" && parameters.contains("content") &&
                parameters["content"].is_string() && parameters["content"].get<string>().empty())
                parameters.erase("content");
            if (sourceStep.contentRequirement == "required" && blank) {
                result.message = "Requested artifact requires populated content. Generate the complete requested content and supply parameters.content in filesystem.create_file; do not create an empty placeholder.";
                result.steps.clear();
                return result;
            }
        }
        json wrapped = {{"id", sourceStep.id}, {"operations", json::array({operation})}};
        unordered_map<string, PrimitiveDefinition> definitions;
        for (const auto& entry : PrimitiveRegistry::getCatalog()) definitions.emplace(entry.first, entry.second);
        if (!parseOperations(wrapped, result.steps.front(), definitions, result.message)) {
            result.steps.clear();
            return result;
        }
        result.valid = true;
        result.message = "Single operation validated against the live primitive registry.";
        return result;
    } catch (const exception& ex) {
        result.message = string("Single-operation validation failed: ") + ex.what();
        result.steps.clear();
        return result;
    }
}

MultiStepPlan PrimitiveMapper::map(const MultiStepPlan& decomposedPlan) {
    if (!decomposedPlan.valid) return failedPlan(decomposedPlan, "Cannot map an invalid decomposed plan.");
    if (decomposedPlan.steps.empty()) return failedPlan(decomposedPlan, "Cannot map an empty plan.");
    if (!generator_) return failedPlan(decomposedPlan, "Primitive mapper has no LLM generator.");

    set<string> sourceIds;
    for (const auto& step : decomposedPlan.steps) {
        if (trim(step.id).empty() || !sourceIds.insert(step.id).second) {
            return failedPlan(decomposedPlan, "Input plan contains an empty or duplicate step ID.");
        }
        if (trim(step.description).empty()) {
            return failedPlan(decomposedPlan, "Input step '" + step.id + "' has an empty description.");
        }
    }

    const string systemPrompt = R"PROMPT(You are Nova's primitive mapper. Map the ENTIRE logical plan to existing registered primitives.
Return only strict JSON in this exact shape:
{"steps":[{"id":"step_1","operations":[{"name":"filesystem.create","parameters":{"name":"Example","location":"desktop"}}]}]}
Rules:
- Use only primitive names present in the supplied live registry catalog.
- For browser navigation, use browser.navigate with the exact HTTP(S) URL and an explicit ready_condition. Use browser.wait_for for a separate page-readiness step. For ready_condition=selector, provide selector only when the goal/context identifies a concrete selector; otherwise use domcontentloaded or load.
- Map every source step to one or more operations. Use the minimum sufficient operations.
- Do not add, remove, reorder, rename, or merge source steps. Copy each source step ID exactly.
- Do not return descriptions, dependencies, explanations, or extra JSON keys. Dependencies are retained by the host.
- Parameters must exactly match each primitive's schema. Include every required parameter and only schema-defined parameters.
- Use only values supported by the user's goal, the step description, or the provided context. Never invent file paths, names, content, applications, or other user-specific values.
- If a required value is genuinely unavailable, do not guess. Return JSON only with {"steps":[]}.
- Do not call tools, execute actions, or invent primitives. This stage only maps the plan.
)PROMPT";

    const string userPrompt =
        "LIVE_PRIMITIVE_REGISTRY:\n" + Utf8::dump(catalogToJson(),"registry",2) +
        "\n\nDECOMPOSED_PLAN (source of truth for step IDs/order/dependencies):\n" +
        Utf8::dump(sourcePlanToJson(decomposedPlan),"source_plan",2) +
        "\n\nReturn the mapped plan as strict JSON only.";

    LLMResponse response;
    try {
        response = generator_(systemPrompt, userPrompt); // Exactly one LLM call for the whole plan.
    } catch (const exception& ex) {
        return failedPlan(decomposedPlan, string("Primitive mapper LLM call failed: ") + ex.what());
    } catch (...) {
        return failedPlan(decomposedPlan, "Primitive mapper LLM call failed with an unknown exception.");
    }

    if (!response.success) {
        return failedPlan(decomposedPlan,
            response.message.empty() ? "Primitive mapper LLM request failed." : response.message);
    }
    if (trim(response.content).empty()) {
        return failedPlan(decomposedPlan, "Primitive mapper returned an empty response.");
    }

    json mapped;
    try {
        mapped = json::parse(response.content);
    } catch (const exception& ex) {
        return failedPlan(decomposedPlan, string("Primitive mapper returned malformed JSON: ") + ex.what());
    }

    if (!mapped.is_object() || mapped.size() != 1 || !mapped.contains("steps") || !mapped["steps"].is_array()) {
        return failedPlan(decomposedPlan, "Mapper response must contain only a top-level 'steps' array.");
    }
    if (mapped["steps"].size() != decomposedPlan.steps.size()) {
        return failedPlan(decomposedPlan, "Mapper must return exactly one mapped entry for every source step.");
    }

    unordered_map<string, PrimitiveDefinition> definitions;
    for (const auto& entry : PrimitiveRegistry::getCatalog()) definitions.emplace(entry.first, entry.second);

    MultiStepPlan result = decomposedPlan;
    for (size_t i = 0; i < result.steps.size(); ++i) {
        if (!parseOperations(mapped["steps"][i], result.steps[i], definitions, result.message)) {
            return failedPlan(decomposedPlan, result.message);
        }
    }

    result.valid = true;
    result.message = "Primitive mapping validated successfully.";
    return result;
}
