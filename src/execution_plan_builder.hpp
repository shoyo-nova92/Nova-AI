#pragma once

#include "execution_plan.hpp"
#include "action_validator.hpp"

class ExecutionPlanBuilder
{
public:
    ExecutionPlan build(
        const Action& action,
        const Capability& capability,
        const ActionParameters& parameters,
        const ActionValidationResult& validation
    );
};