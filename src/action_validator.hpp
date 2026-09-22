#pragma once

#include "action.hpp"
#include "action_parameters.hpp"
#include "capability.hpp"

#include <string>

using namespace std;

struct ActionValidationResult
{
    bool valid;
    string message;
};

class ActionValidator
{
public:
    ActionValidationResult validate(
        const Action& action,
        const Capability& capability,
        const ActionParameters& parameters
    );
};