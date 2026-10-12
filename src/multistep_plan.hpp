#pragma once

#include <string>
#include <vector>
#include "action_parameters.hpp"
#include "primitive_operation.hpp"
#include "context_reference.hpp"

using namespace std;

struct MultiStepAction
{
    string id;
    string description;
    vector<string> dependencies;

    vector<PrimitiveOperation> operations;
    vector<ActionParameters> semanticParameters;

    vector<ContextReference> contextReferences;

    string operation;
    string capability;
    vector<string> parameters;
    bool resolved;
};

struct MultiStepPlan
{
    bool valid;

    string goal;

    vector<MultiStepAction> steps;

    string message;
};