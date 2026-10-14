#include "goal_decomposer.hpp"

#include <iostream>
#include <string>

using namespace std;

int main()
{
    int passed = 0;
    int failed = 0;

    auto check = [&](const string& name, bool condition)
    {
        cout << "[" << name << "] "
             << (condition ? "PASS" : "FAIL") << '\n';

        condition ? ++passed : ++failed;
    };

    const string validJson = R"({
        "steps": [
            {
                "id": "step_1",
                "description": "Create the NovaJava project on the desktop"
            },
            {
                "id": "step_2",
                "description": "Add Main.java to NovaJava",
                "depends_on": ["step_1"]
            },
            {
                "id": "step_3",
                "description": "Open NovaJava in VS Code",
                "depends_on": ["step_1"]
            }
        ]
    })";

    bool called = false;

    GoalDecomposer decomposer(
        [&](const string& systemPrompt, const string& userPrompt)
        {
            called = true;

            check(
                "PROMPT_ONLY_LOGICAL_DECOMPOSITION",
                systemPrompt.find("Goal Decomposer") != string::npos &&
                systemPrompt.find("choose execution mechanisms") != string::npos
            );

            check(
                "ORIGINAL_GOAL_PASSED_TO_LLM",
                userPrompt.find("NovaJava") != string::npos &&
                userPrompt.find("Main.java") != string::npos
            );

            return LLMResponse{true, validJson, ""};
        }
    );

    MultiStepPlan plan = decomposer.decompose(
        "Create NovaJava, add Main.java, and open it in VS Code."
    );

    check("LLM_CALLED", called);
    check("VALID_PLAN_ACCEPTED", plan.valid);
    check("THREE_STEPS_RETURNED", plan.steps.size() == 3);
    check(
        "DEPENDENCIES_PARSED",
        plan.steps.size() == 3 &&
        plan.steps[1].dependencies.size() == 1 &&
        plan.steps[1].dependencies[0] == "step_1"
    );
    check(
        "NO_PRIMITIVES_SELECTED",
        plan.valid &&
        plan.steps[0].operations.empty() &&
        plan.steps[0].operation.empty()
    );

    GoalDecomposer malformed(
        [](const string&, const string&)
        {
            return LLMResponse{true, "{bad json", ""};
        }
    );

    plan = malformed.decompose("Create a folder");

    check("MALFORMED_JSON_REJECTED", !plan.valid);
    check("MALFORMED_PLAN_HAS_NO_STEPS", plan.steps.empty());

    GoalDecomposer badDependency(
        [](const string&, const string&)
        {
            return LLMResponse{
                true,
                R"({"steps":[
                    {"id":"step_1","description":"Create a folder",
                     "depends_on":["step_9"]}
                ]})",
                ""
            };
        }
    );

    plan = badDependency.decompose("Create a folder");

    check("UNKNOWN_DEPENDENCY_REJECTED", !plan.valid);

    GoalDecomposer extraField(
        [](const string&, const string&)
        {
            return LLMResponse{
                true,
                R"({"steps":[
                    {"id":"step_1","description":"Create a folder",
                     "operation":"filesystem.create"}
                ]})",
                ""
            };
        }
    );

    plan = extraField.decompose("Create a folder");

    check("PRIMITIVE_FIELD_REJECTED", !plan.valid);

    GoalDecomposer emptyResponse(
        [](const string&, const string&)
        {
            return LLMResponse{true, R"({"steps":[]})", ""};
        }
    );

    plan = emptyResponse.decompose("Do something");

    check("EMPTY_STEPS_REJECTED", !plan.valid);
    GoalDecomposer failedLLM(
            [](const string&, const string&) {
                return LLMResponse{
                    false,
                    "",
                    "simulated API failure"
                };
            }
        );

        const auto failurePlan = failedLLM.decompose("Create a folder");

        check(
            "LLM_FAILURE_REJECTED",
            !failurePlan.valid
        );

        check(
            "LLM_FAILURE_HAS_NO_STEPS",
            failurePlan.steps.empty()
        );

        check(
            "LLM_FAILURE_MESSAGE",
            failurePlan.message.find("simulated API failure") != string::npos
        );
    cout << "\nGOAL DECOMPOSER CONTRACT TEST\n"
         << "Passed: " << passed << '\n'
         << "Failed: " << failed << '\n';

    return failed == 0 ? 0 : 1;

    
    
}