#pragma once

#include "goal_decomposer.hpp"
#include "primitive_mapper.hpp"
#include "primitive_execution_engine.hpp"

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
};

class Branch3Runtime {
public:
    Branch3Runtime();
    Branch3Runtime(GoalDecomposerLLM decomposerGenerator,
                   PrimitiveMapperLLM mapperGenerator);

    // Performs exactly one decomposition call and one whole-plan mapping call,
    // validates the complete mapped plan, then executes operations in plan order.
    Branch3RuntimeResult run(const std::string& goal);

private:
    GoalDecomposer decomposer_;
    PrimitiveMapper mapper_;
};
