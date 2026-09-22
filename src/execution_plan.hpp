#pragma once

#include "action.hpp"
#include "action_parameters.hpp"
#include "capability.hpp"

#include <string>

using namespace std;

struct ExecutionPlan
{
    bool valid;

    string type;

    Action action;

    Capability capability;

    ActionParameters parameters;

    string message;
};