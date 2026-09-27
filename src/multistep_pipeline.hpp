#pragma once

#include "multistep_plan.hpp"
#include "goal_decomposer.hpp"
#include "dependency_resolver.hpp"
#include "action_primitive_resolver.hpp"
#include "capability_resolver.hpp"
#include "parameter_resolver.hpp"
#include "context_resolver.hpp"

#include <string>

using namespace std;

enum class MultiStepPipelineStatus
{
    SUCCESS,
    CLARIFICATION_REQUIRED,
    FAILED
};

struct MultiStepPipelineResult
{
    MultiStepPipelineStatus status;

    MultiStepPlan plan;

    string message;

    string clarificationQuestion;
};

class MultiStepPipeline
{
public:

    MultiStepPipelineResult run(
        const string& goal
    );
};