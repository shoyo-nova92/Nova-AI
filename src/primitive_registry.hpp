#pragma once

#include "primitive_operation.hpp"
#include "primitive_parameter_schema.hpp"

#include <optional>
#include <string>
#include <vector>

using namespace std;

struct PrimitiveDefinition
{
    PrimitiveOperationType type;
    optional<PrimitiveParameterSchema> parameters;
};

class PrimitiveRegistry
{
public:
    static bool isSupported(
        const string& primitiveName
    );

    static bool resolveType(
        const string& primitiveName,
        PrimitiveOperationType& type
    );

    static bool resolveDefinition(
        const string& primitiveName,
        PrimitiveDefinition& definition
    );

    static vector<string> getSupportedNames();
};