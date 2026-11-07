#pragma once

#include <string>
#include <vector>
#include <optional>
#include "action_parameters.hpp"
#include "primitive_operation.hpp"
#include "context_reference.hpp"
#include "llm_client.hpp"

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

    // Semantic artifact intent, supplied by goal interpretation, never by filename.
    // Optional declarative postcondition; untyped steps require observed planner verification.
    string semanticPostconditionJson; // Canonical registry-validated declaration; artifact fields remain compatible.
    string postconditionKind;
    string artifactName;
    string artifactLocation;
    string contentRequirement = "unspecified";
    string conditionJson;
    optional<ContextReference> artifactParentReference;
};

struct MultiStepPlan
{
    bool valid;

    string goal;

    vector<MultiStepAction> steps;

    string message;
    LLMFailureCategory failureCategory = LLMFailureCategory::NONE;
};
