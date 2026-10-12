#pragma once

#include <string>

using namespace std;

enum class ParameterValueType
{
    NONE,
    LITERAL,
    CONTEXT_REFERENCE
};

struct ContextParameterReference
{
    string stepId;
    string output;
};

struct ParameterValue
{
    ParameterValueType type = ParameterValueType::NONE;

    string value;

    ContextParameterReference reference;
};

struct ActionParameters
{
    ParameterValue name;
    ParameterValue location;
    ParameterValue application;
    ParameterValue target;
    ParameterValue content;
};