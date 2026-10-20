#pragma once

#include "goal_decomposer.hpp"
#include "primitive_mapper.hpp"
#include "primitive_execution_engine.hpp"
#include "next_action_planner.hpp"
#include <string>
#include <vector>

struct Branch3OperationResult {
    std::string stepId;
    std::size_t operationIndex = 0;
    ExecutionResult execution;
};

struct Branch3RuntimeResult {
    bool success = false;
    bool planningSuccess = false;
    bool executionStarted = false;
    std::string message;
    MultiStepPlan decomposedPlan{};
    MultiStepPlan mappedPlan{};
    std::vector<Branch3OperationResult> operationResults;
    std::string stateLog;
};

class Branch3Runtime {
public:
    Branch3Runtime();
    Branch3Runtime(GoalDecomposerLLM decomposerGenerator,
                   PrimitiveMapperLLM mapperGenerator,
                   NextActionLLM plannerGenerator = {});

    // Decomposes once, then repeatedly observes, plans one operation, validates, and executes.
    Branch3RuntimeResult run(const std::string& goal);

private:
    GoalDecomposer decomposer_;
    PrimitiveMapper mapper_;
    NextActionPlanner planner_;
};
