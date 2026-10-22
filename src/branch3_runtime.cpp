#include "branch3_runtime.hpp"
#include "primitive_registry.hpp"
#include "next_action_planner.hpp"
#include "state_manager.hpp"
#include <nlohmann/json.hpp>
#include <exception>
#include <algorithm>
#include <cctype>
#include <sstream>
#include <utility>
#include <string>
#include <vector>

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

std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

bool browserObservationNeeded(const MultiStepPlan& guide) {
    for (const auto& step : guide.steps) {
        const std::string description = lower(step.description);
        if (description.find("browser") != std::string::npos ||
            description.find("website") != std::string::npos ||
            description.find("web page") != std::string::npos ||
            description.find("http://") != std::string::npos ||
            description.find("https://") != std::string::npos) return true;
    }
    return false;
}

std::string expectedCreatedFolderName(const std::string& description) {
    const std::string normalized = lower(description);
    const std::vector<std::string> prefixes = {
        "create folder '", "create directory '", "create a folder named '",
        "create a directory named '", "create folder \"", "create directory \"",
        "create a folder named \"", "create a directory named \""
    };
    for (const auto& prefix : prefixes) {
        const auto start = normalized.find(prefix);
        if (start == std::string::npos) continue;
        const std::size_t valueStart = start + prefix.size();
        const char quote = prefix.back();
        const auto end = description.find(quote, valueStart);
        if (end != std::string::npos && end > valueStart)
            return description.substr(valueStart, end - valueStart);
    }

    // Fallback for a decomposer that describes the intended target as a path.
    const auto at = normalized.find(" at ");
    if (at != std::string::npos) {
        std::string path = description.substr(at + 4);
        while (!path.empty() && (path.back() == '.' || path.back() == ',' ||
                                 path.back() == ';' || path.back() == ' ')) path.pop_back();
        const auto separator = path.find_last_of("/\\");
        if (separator != std::string::npos && separator + 1 < path.size())
            return path.substr(separator + 1);
    }
    return {};
}

std::vector<std::string> expectedParentLocationTokens(const std::string& description) {
    const std::string normalized = lower(description);
    const auto at = normalized.find(" at ");
    if (at == std::string::npos) return {};
    std::string path = description.substr(at + 4);
    while (!path.empty() && (path.back() == '.' || path.back() == ',' ||
                             path.back() == ';' || path.back() == ' ')) path.pop_back();
    const auto separator = path.find_last_of("/\\");
    if (separator == std::string::npos) return {};
    path.resize(separator);

    std::vector<std::string> tokens;
    std::string token;
    auto flush = [&]() {
        if (!token.empty()) {
            tokens.push_back(lower(token));
            token.clear();
        }
    };
    for (unsigned char c : path) {
        if (std::isalnum(c)) token.push_back(static_cast<char>(std::tolower(c)));
        else flush();
    }
    flush();

    auto alias = tokens.end();
    for (auto it = tokens.begin(); it != tokens.end(); ++it) {
        if (*it == "desktop" || *it == "documents" || *it == "downloads")
            alias = it;
    }
    if (alias != tokens.end()) {
        tokens.erase(tokens.begin(), alias + 1);
    } else if (tokens.size() > 3) {
        // Absolute paths may not contain a known-folder alias. Retain the
        // destination's trailing path context, not machine/user-name tokens.
        tokens.erase(tokens.begin(), tokens.end() - 3);
    }
    return tokens;
}

bool locationContainsExpectedTokens(const std::string& location,
                                    const std::vector<std::string>& expected) {
    std::vector<std::string> actual;
    std::string token;
    auto flush = [&]() {
        if (!token.empty()) {
            actual.push_back(lower(token));
            token.clear();
        }
    };
    for (unsigned char c : location) {
        if (std::isalnum(c)) token.push_back(static_cast<char>(std::tolower(c)));
        else flush();
    }
    flush();

    for (const auto& clue : expected) {
        const bool found = std::any_of(actual.begin(), actual.end(), [&](const std::string& value) {
            return value == clue || (clue.size() >= 3 && value.size() >= 3 &&
                (value.starts_with(clue) || clue.starts_with(value)));
        });
        if (!found) return false;
    }
    return true;
}

std::string operationSignature(const PrimitiveOperation& operation,
                               const std::string& errorCategory) {
    const auto& p = operation.parameters;
    return operation.name + "|" + p.name + "|" + p.location + "|" + p.target +
        "|" + p.source + "|" + p.destination + "|" + p.newName + "|" +
        p.query + "|" + p.entryType + "|reason=" + errorCategory;
}

std::string operationKey(const PrimitiveOperation& operation) {
    const std::string signature = operationSignature(operation, "none");
    return signature.substr(0, signature.rfind("|reason="));
}

std::string resultEvidence(const ExecutionResult& execution) {
    std::string evidence = execution.message;
    if (!execution.resolvedPath.empty()) evidence += " | resolved_path=" + execution.resolvedPath;
    if (!execution.discoveredPaths.empty()) {
        evidence += " | discovered_paths=";
        for (std::size_t index = 0; index < execution.discoveredPaths.size(); ++index) {
            if (index) evidence += ", ";
            evidence += execution.discoveredPaths[index];
        }
    }
    return evidence;
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
    const bool observeBrowser = browserObservationNeeded(result.decomposedPlan);
    std::string lastBrowserAction;
    std::string snapshotBeforeLastBrowserAction;

    for (unsigned iteration = 0; iteration < MAX_ITERATIONS; ++iteration) {
        state.nextIteration();
        std::string observed;
        std::string currentSnapshot;
        if (observeBrowser) {
            // Browser operations require a fresh live DOM observation. Filesystem
            // goals instead use explicit inspect/list/search evidence.
            PrimitiveOperation observe{};
            observe.name = "browser.observe"; observe.type = PrimitiveOperationType::BROWSER_OBSERVE; observe.resolved = true;
            auto observation = engine.execute(observe);
            observed = observation.message;
            if (observation.status != ExecutionStatus::SUCCESS) observed = "Browser observation unavailable: " + observation.message;
            state.setObservation(observed);
            currentSnapshot = snapshotSignature(observed);
        } else {
            observed = "No automatic filesystem observation. Use filesystem.inspect, filesystem.list_directory, or filesystem.search when location evidence is needed.";
            state.setObservation(observed);
        }
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

        // A missing destination is not permission to manufacture its ancestors.
        // The create target must match the folder explicitly requested by the
        // selected logical step; location path segments are not new targets.
        if (operation.type == PrimitiveOperationType::FILESYSTEM_CREATE &&
            operation.name == "filesystem.create") {
            const std::string expected = expectedCreatedFolderName(sourceStep->description);
            const auto expectedParent = expectedParentLocationTokens(sourceStep->description);
            const bool targetMismatch = !expected.empty() &&
                lower(expected) != lower(operation.parameters.name);
            const bool locationMismatch = !expectedParent.empty() &&
                !locationContainsExpectedTokens(operation.parameters.location, expectedParent);
            if (targetMismatch || locationMismatch) {
                const std::string category = "planner_target_mismatch";
                state.recordFailure(operationSignature(operation, category));
                std::string diagnostic = "Rejected filesystem.create for '" +
                    operation.parameters.name + "' at '" + operation.parameters.location + "'.";
                if (targetMismatch)
                    diagnostic += " The selected step requests folder '" + expected + "'.";
                if (locationMismatch)
                    diagnostic += " The destination does not contain the parent-location clues from the selected step.";
                diagnostic += " Resolve the existing destination; do not create missing path ancestors.";
                state.addHistory(diagnostic);
                result.stateLog += "[VALIDATION] rejected: " + diagnostic + "\n";
                continue;
            }
        }

        if (state.repeatedFailure(operationKey(operation))) {
            result.message = "Branch 3 blocked: the same operation with the same normalized arguments and failure category failed three times consecutively."; return result;
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
            const std::string evidence = resultEvidence(operationResult.execution);
            state.addHistory("Succeeded " + operation.name + ": " + evidence);
            state.setObservation("Filesystem/action result: " + evidence);
            result.mappedPlan.steps.push_back(mapped.steps.front());
            result.stateLog += "[EXECUTION] success " + operation.name + ": " + evidence + "\n";
        } else {
            const std::string failureSignature = operationSignature(operation,
                operationResult.execution.failureCategory);
            state.recordFailure(failureSignature);
            const std::string evidence = resultEvidence(operationResult.execution);
            state.addHistory("Failed " + operation.name + " [" +
                operationResult.execution.failureCategory + "]: " + evidence);
            state.setObservation("Filesystem/action failure: " + evidence);
            result.stateLog += "[EXECUTION] failed " + operation.name + ": " + evidence + "\n";
        }
    }
    result.message = "Branch 3 stopped after reaching the maximum of 12 observe-plan-execute iterations without verified completion.";
    return result;
}
