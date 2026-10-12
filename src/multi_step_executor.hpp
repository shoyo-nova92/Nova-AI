#pragma once

#include "execution_plan.hpp"
#include "execution_result.hpp"

#include <vector>
#include <string>

using namespace std;

struct MultiStepExecutionResult
{
    bool success;
    vector<ExecutionResult> stepResults;
    string message;
};

class MultiStepExecutor
{
public:
    MultiStepExecutionResult execute(
        const ExecutionPlan& plan
    );
};