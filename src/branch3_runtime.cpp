#include "branch3_runtime.hpp"
#include "primitive_registry.hpp"
#include "next_action_planner.hpp"
#include "state_manager.hpp"
#include <nlohmann/json.hpp>
#include <exception>
#include <sstream>
#include <utility>
#include <string>

namespace {
std::string liveCatalog() {
    nlohmann::json catalog = nlohmann::json::array();
    for (const auto& [name, def] : PrimitiveRegistry::getCatalog()) {
        nlohmann::json item = {{"name",name},{"description",def.description},{"capability",def.capability},{"parameters",nlohmann::json::array()}};
        if (def.parameters) for (const auto& f : def.parameters->fields) {
            std::string requirement = f.requirement == ParameterRequirement::REQUIRED ? "required" :
                (f.requirement == ParameterRequirement::OPTIONAL ? "optional" : "not_applicable");
            item["parameters"].push_back({{"name",f.name},{"requirement",requirement},{"description",f.description},{"allowed_values",f.allowedValues}});
        }
        catalog.push_back(std::move(item));
    }
    return catalog.dump(2);
}
const MultiStepAction* findStep(const MultiStepPlan& plan, const std::string& id) {
    for (const auto& step : plan.steps) if (step.id == id) return &step;
    return nullptr;
}
std::string snapshotSignature(const std::string& observationMessage) {
    const std::string marker = "Browser observation: ";
    const auto markerPos = observationMessage.find(marker);
    if (markerPos == std::string::npos) return observationMessage;
    const std::string payload = observationMessage.substr(markerPos + marker.size());
    try {
        const auto snapshot = nlohmann::json::parse(payload);
        nlohmann::json stable = nlohmann::json::object();
        for (const char* key : {"url", "title", "page_kind", "visible_text", "elements", "focus"})
            if (snapshot.contains(key)) stable[key] = snapshot[key];
        return stable.dump();
    } catch (...) {
        return observationMessage;
    }
}
}

Branch3Runtime::Branch3Runtime() = default;
Branch3Runtime::Branch3Runtime(GoalDecomposerLLM decomposerGenerator, PrimitiveMapperLLM mapperGenerator, NextActionLLM plannerGenerator)
    : decomposer_(std::move(decomposerGenerator)), mapper_(std::move(mapperGenerator)),
      planner_(plannerGenerator ? NextActionPlanner(std::move(plannerGenerator)) : NextActionPlanner()) {}

Branch3RuntimeResult Branch3Runtime::run(const std::string& goal) {
    Branch3RuntimeResult result;
    if (goal.find_first_not_of(" \t\r\n") == std::string::npos) { result.message = "Goal is empty."; return result; }
    try { result.decomposedPlan = decomposer_.decompose(goal); }
    catch (const std::exception& e) { result.message = std::string("Goal decomposition failed: ") + e.what(); return result; }
    catch (...) { result.message = "Goal decomposition failed with an unknown exception."; return result; }
    if (!result.decomposedPlan.valid || result.decomposedPlan.steps.empty()) {
        result.message = "Goal decomposition failed: " + result.decomposedPlan.message; return result;
    }

    StateManager state(goal);
    PrimitiveExecutionEngine engine;
    result.mappedPlan = result.decomposedPlan;
    result.mappedPlan.valid = true;
    result.mappedPlan.steps.clear();
    result.planningSuccess = true;
    result.executionStarted = true;
    constexpr unsigned MAX_ITERATIONS = 12;
    const std::string catalog = liveCatalog();
    std::string lastBrowserAction;
    std::string snapshotBeforeLastBrowserAction;

    for (unsigned iteration = 0; iteration < MAX_ITERATIONS; ++iteration) {
        state.nextIteration();
        // Refresh the browser environment before every planning decision.
        PrimitiveOperation observe{};
        observe.name = "browser.observe"; observe.type = PrimitiveOperationType::BROWSER_OBSERVE; observe.resolved = true;
        auto observation = engine.execute(observe);
        std::string observed = observation.message;
        if (observation.status != ExecutionStatus::SUCCESS) observed = "Browser observation unavailable: " + observation.message;
        state.setObservation(observed);
        const std::string currentSnapshot = snapshotSignature(observed);
        result.stateLog += state.render() + "\n";

        if (!lastBrowserAction.empty()) {
            if (currentSnapshot == snapshotBeforeLastBrowserAction) {
                const std::string diagnostic = "No observable state change after " + lastBrowserAction +
                    "; stopping to prevent repeated blind browser actions.";
                state.addHistory(diagnostic);
                result.stateLog += "[PROGRESS CHECK] " + diagnostic + "\n";
                result.message = "Branch 3 stopped: " + diagnostic;
                return result;
            }
            // The prior action changed the observed page; a new action may now be planned.
            lastBrowserAction.clear();
            snapshotBeforeLastBrowserAction.clear();
        }

        NextActionDecision decision = planner_.choose(goal, result.decomposedPlan, state.render(), catalog);
        result.stateLog += "[PLANNER] " + decision.status + ": " + decision.message + "\n";
        if (decision.status == "done") {
            result.success = true;
            result.message = "Branch 3 goal reported complete by planner after fresh environment observation: " + decision.message;
            return result;
        }
        if (decision.status == "ask_user" || decision.status == "blocked") {
            result.message = (decision.status == "ask_user" ? "Needs user input: " : "Branch 3 blocked: ") + decision.message;
            return result;
        }
        if (decision.status != "operation") { result.message = "Branch 3 blocked: planner returned no actionable decision."; return result; }
        const MultiStepAction* sourceStep = findStep(result.decomposedPlan, decision.stepId);
        if (!sourceStep) { result.message = "Branch 3 blocked: planner selected an unknown guide step ID '" + decision.stepId + "'."; return result; }
        MultiStepPlan mapped = mapper_.mapStep(*sourceStep, decision.operationJson);
        if (!mapped.valid || mapped.steps.empty() || mapped.steps.front().operations.size() != 1) {
            state.addHistory("Rejected planner operation: " + mapped.message);
            result.stateLog += "[VALIDATION] rejected: " + mapped.message + "\n";
            result.message = "Planner operation failed registry/schema validation: " + mapped.message;
            return result;
        }
        const PrimitiveOperation& operation = mapped.steps.front().operations.front();
        if (state.repeatedFailure(operation.name)) {
            result.message = "Branch 3 blocked: operation '" + operation.name + "' failed three times consecutively."; return result;
        }
        if (operation.name == "browser.click" || operation.name == "browser.type") {
            lastBrowserAction = operation.name;
            snapshotBeforeLastBrowserAction = currentSnapshot;
        }

        Branch3OperationResult operationResult;
        operationResult.stepId = sourceStep->id;
        operationResult.operationIndex = 0;
        operationResult.execution = engine.execute(operation);
        result.operationResults.push_back(operationResult);
        if (operationResult.execution.status == ExecutionStatus::SUCCESS) {
            state.recordSuccess();
            state.addHistory("Succeeded " + operation.name + ": " + operationResult.execution.message);
            result.mappedPlan.steps.push_back(mapped.steps.front());
            result.stateLog += "[EXECUTION] success " + operation.name + ": " + operationResult.execution.message + "\n";
        } else {
            state.recordFailure(operation.name);
            state.addHistory("Failed " + operation.name + ": " + operationResult.execution.message);
            result.stateLog += "[EXECUTION] failed " + operation.name + ": " + operationResult.execution.message + "\n";
        }
    }
    result.message = "Branch 3 stopped after reaching the maximum of 12 observe-plan-execute iterations without verified completion.";
    return result;
}
