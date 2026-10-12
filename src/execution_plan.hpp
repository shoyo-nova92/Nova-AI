#pragma once

#include "action.hpp"
#include "action_parameters.hpp"
#include "capability.hpp"
#include "primitive_operation.hpp"

#include <string>
#include <vector>

using namespace std;

struct ExecutionPlanStep
{
    string id;

    vector<string> dependencies;

    PrimitiveOperation operation;

    bool valid;
};

struct ExecutionPlan
{
    bool valid;

    string type;

    // B2 single-action execution
    Action action;

    Capability capability;

    PrimitiveParameters parameters;

    // B3 multi-step execution
    vector<ExecutionPlanStep> steps;

    string message;
};