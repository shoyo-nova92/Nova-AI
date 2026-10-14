#include "goal_decomposer.hpp"
#include "primitive_mapper.hpp"
#include "primitive_registry.hpp"

#include <iostream>
#include <set>
#include <string>
#include <unordered_set>

using namespace std;

namespace {
const string TEST_GOAL =
    "Create a folder named `NovaJava` on my desktop, create a file named "
    "`Main.java` inside it, and open the project in VS Code.";

void printPlan(const string& title, const MultiStepPlan& plan, bool mapped) {
    cout << "\n" << title << "\n";
    cout << "Valid: " << (plan.valid ? "true" : "false") << "\n";
    cout << "Message: " << plan.message << "\n";
    cout << "Step count: " << plan.steps.size() << "\n";

    for (const auto& step : plan.steps) {
        cout << "\n  [" << step.id << "] " << step.description << "\n";
        cout << "    Depends on: ";
        if (step.dependencies.empty()) {
            cout << "(none)";
        } else {
            for (size_t i = 0; i < step.dependencies.size(); ++i) {
                if (i) cout << ", ";
                cout << step.dependencies[i];
            }
        }
        cout << "\n";

        if (mapped) {
            for (const auto& operation : step.operations) {
                cout << "    -> " << operation.name
                     << " [resolved=" << (operation.resolved ? "true" : "false") << "]\n";
                const auto& p = operation.parameters;
                if (!p.name.empty()) cout << "       name: " << p.name << "\n";
                if (!p.location.empty()) cout << "       location: " << p.location << "\n";
                if (!p.application.empty()) cout << "       application: " << p.application << "\n";
                if (!p.target.empty()) cout << "       target: " << p.target << "\n";
                if (!p.content.empty()) cout << "       content: " << p.content << "\n";
                if (!p.source.empty()) cout << "       source: " << p.source << "\n";
                if (!p.destination.empty()) cout << "       destination: " << p.destination << "\n";
            }
        }
    }
}

bool sameStructure(const MultiStepPlan& before, const MultiStepPlan& after, string& error) {
    if (before.steps.size() != after.steps.size()) {
        error = "Mapper changed the number of logical steps.";
        return false;
    }

    for (size_t i = 0; i < before.steps.size(); ++i) {
        if (before.steps[i].id != after.steps[i].id) {
            error = "Mapper changed/reordered step ID at index " + to_string(i) + ".";
            return false;
        }
        if (before.steps[i].dependencies != after.steps[i].dependencies) {
            error = "Mapper changed dependencies for " + before.steps[i].id + ".";
            return false;
        }
        if (after.steps[i].operations.empty()) {
            error = "Mapped step " + after.steps[i].id + " has no operations.";
            return false;
        }
        for (const auto& operation : after.steps[i].operations) {
            if (!operation.resolved) {
                error = "Operation was not marked resolved: " + operation.name;
                return false;
            }
            if (!PrimitiveRegistry::isSupported(operation.name)) {
                error = "Mapped operation is absent from registry: " + operation.name;
                return false;
            }
        }
    }
    return true;
}
}

int main() {
    cout << "============================================================\n";
    cout << "NOVA BRANCH 3 — LIVE LLM PLANNING TEST (NO EXECUTION)\n";
    cout << "============================================================\n";
    cout << "Goal: " << TEST_GOAL << "\n";
    cout << "\nThis test makes real LLM requests in both planning stages.\n";
    cout << "It does NOT execute filesystem or application operations.\n";

    // Stage 1: real LLM call through GoalDecomposer's production LLM client.
    GoalDecomposer decomposer;
    const MultiStepPlan decomposed = decomposer.decompose(TEST_GOAL);
    printPlan("[STAGE 1: GOAL DECOMPOSITION]", decomposed, false);

    if (!decomposed.valid) {
        cerr << "\nFAIL: Goal decomposition failed: " << decomposed.message << "\n";
        return 1;
    }
    if (decomposed.steps.empty()) {
        cerr << "\nFAIL: Decomposer returned no logical steps.\n";
        return 1;
    }

    // Stage 2: one real LLM call for the entire decomposed plan.
    PrimitiveMapper mapper;
    const MultiStepPlan mapped = mapper.map(decomposed);
    printPlan("[STAGE 2: PRIMITIVE MAPPING]", mapped, true);

    if (!mapped.valid) {
        cerr << "\nFAIL: Primitive mapping failed: " << mapped.message << "\n";
        return 2;
    }

    string structureError;
    if (!sameStructure(decomposed, mapped, structureError)) {
        cerr << "\nFAIL: " << structureError << "\n";
        return 3;
    }

    // Explicitly check dependencies refer only to IDs in the original plan.
    unordered_set<string> ids;
    for (const auto& step : decomposed.steps) ids.insert(step.id);
    for (const auto& step : mapped.steps) {
        for (const auto& dependency : step.dependencies) {
            if (!ids.contains(dependency)) {
                cerr << "\nFAIL: Unknown dependency '" << dependency
                     << "' in step " << step.id << ".\n";
                return 4;
            }
        }
    }

    cout << "\n============================================================\n";
    cout << "LIVE PLANNING RESULT: PASS\n";
    cout << "- Goal decomposition used the real LLM client.\n";
    cout << "- Primitive mapping used the real LLM client.\n";
    cout << "- Every mapped primitive exists in the live registry.\n";
    cout << "- Every mapped operation is marked resolved.\n";
    cout << "- Step IDs, ordering, and dependencies were preserved.\n";
    cout << "- NO OPERATIONS WERE EXECUTED.\n";
    cout << "============================================================\n";
    return 0;
}
