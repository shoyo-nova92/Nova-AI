#pragma once

#include "execution_plan.hpp"
#include "action_validator.hpp"
#include "multistep_plan.hpp"

#include <vector>

class ExecutionPlanBuilder
{
public:

    ExecutionPlan build(
        const Action& action,
        const Capability& capability,
        const ActionParameters& parameters,
        const ActionValidationResult& validation
    );

    ExecutionPlan build(
        const MultiStepPlan& plan,
        const vector<string>& executionOrder
    );
};