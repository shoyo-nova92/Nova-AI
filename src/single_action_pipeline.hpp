#pragma once

#include "action_parser.hpp"
#include "capability.hpp"
#include "action_parameters.hpp"
#include "action_validator.hpp"
#include "execution_plan.hpp"

#include <string>

using namespace std;

struct SingleActionResult
{
    bool success;

    Action action;

    Capability capability;

    ActionParameters parameters;

    ActionValidationResult validation;

    ExecutionPlan executionPlan;

    string message;
};

class SingleActionPipeline
{
public:
    SingleActionResult process(
        const string& input
    );
};