#pragma once

#include "primitive_operation.hpp"

#include <string>
#include <vector>

using namespace std;

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

    static vector<string> getSupportedNames();
};