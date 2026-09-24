#pragma once

#include "execution_plan.hpp"
#include "execution_result.hpp"

class ExecutionEngine
{
public:
    ExecutionResult execute(
        const ExecutionPlan& plan
    );
};