#pragma once
#include <string>
#include <vector>

enum class ParameterRequirement { NOT_APPLICABLE, REQUIRED, OPTIONAL };
enum class PrimitiveParameterType {
    STRING, TEXT, FILE_PATH, DIRECTORY_PATH, APPLICATION, URL, KEY,
    MODIFIER_LIST, ENUM, INTEGER, NUMBER, BOOLEAN
};

struct PrimitiveParameterDefinition {
    std::string name;
    PrimitiveParameterType type = PrimitiveParameterType::STRING;
    ParameterRequirement requirement = ParameterRequirement::REQUIRED;
    std::string description;
    std::vector<std::string> allowedValues;
    bool hasMinimum = false;
    double minimum = 0.0;
    bool hasMaximum = false;
    double maximum = 0.0;
};

struct PrimitiveParameterSchema {
    // Compatibility fields for the old ParameterResolver. `fields` is authoritative
    // for the new Branch 3 schema-driven mapper.
    ParameterRequirement name = ParameterRequirement::NOT_APPLICABLE;
    ParameterRequirement location = ParameterRequirement::NOT_APPLICABLE;
    ParameterRequirement application = ParameterRequirement::NOT_APPLICABLE;
    ParameterRequirement target = ParameterRequirement::NOT_APPLICABLE;
    ParameterRequirement content = ParameterRequirement::NOT_APPLICABLE;
    std::vector<PrimitiveParameterDefinition> fields;
};
