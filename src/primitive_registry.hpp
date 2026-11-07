#pragma once
#include "primitive_operation.hpp"
#include "primitive_parameter_schema.hpp"
#include <optional>
#include <string>
#include <utility>
#include <vector>
using namespace std;

struct PrimitiveOutputDefinition {
    string name;
    string type;
    string description;
};

struct PrimitiveDefinition {
    PrimitiveOperationType type{};
    optional<PrimitiveParameterSchema> parameters;
    string description;
    string capability;
    vector<PrimitiveOutputDefinition> outputs;
};

class PrimitiveRegistry {
public:
    static bool isSupported(const string& primitiveName);
    static bool resolveType(const string& primitiveName, PrimitiveOperationType& type);
    static bool resolveDefinition(const string& primitiveName, PrimitiveDefinition& definition);
    static vector<string> getSupportedNames();
    static vector<pair<string, PrimitiveDefinition>> getCatalog();
};
