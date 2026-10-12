#pragma once

enum class ParameterRequirement
{
    NOT_APPLICABLE,
    REQUIRED,
    OPTIONAL
};

struct PrimitiveParameterSchema
{
    ParameterRequirement name;
    ParameterRequirement location;
    ParameterRequirement application;
    ParameterRequirement target;
    ParameterRequirement content;
};