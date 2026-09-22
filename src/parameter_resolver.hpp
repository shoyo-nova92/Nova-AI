#pragma once

#include "action.hpp"
#include "action_parameters.hpp"
#include "capability.hpp"

#include <string>

using namespace std;

struct ParameterResolverResult
{
    bool success;
    ActionParameters parameters;
    string message;
};

class ParameterResolver
{
public:
    ParameterResolverResult resolve(
        const string& input,
        const Action& action,
        const Capability& capability
    );
};