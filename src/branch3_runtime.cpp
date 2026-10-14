#include "branch3_runtime.hpp"
#include "primitive_registry.hpp"

#include <exception>
#include <unordered_set>
#include <utility>

namespace {
bool validateMappedPlan(const MultiStepPlan& plan, std::string& error) {
    if (!plan.valid) {
        error = "Mapped plan is invalid: " + plan.message;
        return false;
    }
    if (plan.steps.empty()) {
        error = "Mapped plan contains no steps.";
        return false;
    }

    std::unordered_set<std::string> completedIds;
    for (const auto& step : plan.steps) {
        if (step.id.empty() || !completedIds.insert(step.id).second) {
            error = "Mapped plan contains an empty or duplicate step ID.";
            return false;
        }
        if (step.description.empty() || !step.resolved || step.operations.empty()) {
            error = "Mapped step '" + step.id + "' is unresolved or has no operations.";
            return false;
        }
        for (const auto& dependency : step.dependencies) {
            if (dependency == step.id || !completedIds.contains(dependency)) {
                error = "Step '" + step.id + "' has a dependency that is not an earlier step: " + dependency;
                return false;
            }
        }
        for (const auto& operation : step.operations) {
            PrimitiveDefinition definition;
            if (!operation.resolved ||
                !PrimitiveRegistry::resolveDefinition(operation.name, definition)) {
                error = "Step '" + step.id + "' contains an unresolved or unregistered primitive: " + operation.name;
                return false;
            }
            if (operation.type != definition.type) {
                error = "Primitive type mismatch for '" + operation.name + "'.";
                return false;
            }
        }
    }
    return true;
}
}

Branch3Runtime::Branch3Runtime() = default;

Branch3Runtime::Branch3Runtime(GoalDecomposerLLM decomposerGenerator,
                               PrimitiveMapperLLM mapperGenerator)
    : decomposer_(std::move(decomposerGenerator)),
      mapper_(std::move(mapperGenerator)) {}

Branch3RuntimeResult Branch3Runtime::run(const std::string& goal) {
    Branch3RuntimeResult result;
    if (goal.find_first_not_of(" \t\r\n") == std::string::npos) {
        result.message = "Goal is empty.";
        return result;
    }

    try {
        result.decomposedPlan = decomposer_.decompose(goal);
    } catch (const std::exception& e) {
        result.message = std::string("Goal decomposition failed: ") + e.what();
        return result;
    } catch (...) {
        result.message = "Goal decomposition failed with an unknown exception.";
        return result;
    }
    if (!result.decomposedPlan.valid) {
        result.message = "Goal decomposition failed: " + result.decomposedPlan.message;
        return result;
    }

    result.mappedPlan = mapper_.map(result.decomposedPlan);
    if (!result.mappedPlan.valid) {
        result.message = "Primitive mapping failed: " + result.mappedPlan.message;
        return result;
    }

    std::string validationError;
    if (!validateMappedPlan(result.mappedPlan, validationError)) {
        result.message = "Mapped plan validation failed: " + validationError;
        return result;
    }
    result.planningSuccess = true;
    result.executionStarted = true;

    PrimitiveExecutionEngine engine;
    for (const auto& step : result.mappedPlan.steps) {
        for (std::size_t i = 0; i < step.operations.size(); ++i) {
            Branch3OperationResult operationResult;
            operationResult.stepId = step.id;
            operationResult.operationIndex = i;
            operationResult.execution = engine.execute(step.operations[i]);
            result.operationResults.push_back(operationResult);

            if (operationResult.execution.status != ExecutionStatus::SUCCESS) {
                result.message = "Execution stopped at " + step.id + ", primitive " +
                    step.operations[i].name + ": " + operationResult.execution.message;
                return result;
            }
        }
    }

    result.success = true;
    result.message = "Branch 3 completed: goal decomposed, mapped, validated, and executed.";
    return result;
}
