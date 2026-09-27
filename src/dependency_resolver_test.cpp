#include "dependency_resolver.hpp"

#include <iostream>

using namespace std;

void printResult(
    const string& name,
    const DependencyResolutionResult& result
)
{
    cout << endl;
    cout << "[" << name << "]" << endl;

    cout << "Valid: "
         << (result.valid ? "true" : "false")
         << endl;

    cout << "Message: "
         << result.message
         << endl;

    cout << "Execution Order: ";

    if (result.executionOrder.empty())
    {
        cout << "[none]";
    }
    else
    {
        for (const auto& step :
             result.executionOrder)
        {
            cout << step << " ";
        }
    }

    cout << endl;
}

MultiStepPlan makeBasePlan()
{
    MultiStepPlan plan;

    plan.valid = true;
    plan.goal = "Test multi-step dependency graph";
    plan.message = "";

    return plan;
}

int main()
{
    DependencyResolver resolver;

    int passed = 0;
    int total = 0;

    // --------------------------------------------------
    // TEST 1: Linear dependency chain
    // --------------------------------------------------

    {
        total++;

        MultiStepPlan plan =
            makeBasePlan();

        MultiStepAction step1;
        step1.id = "step_1";
        step1.description = "First step";
        step1.resolved = false;

        MultiStepAction step2;
        step2.id = "step_2";
        step2.description = "Second step";
        step2.dependencies = {"step_1"};
        step2.resolved = false;

        MultiStepAction step3;
        step3.id = "step_3";
        step3.description = "Third step";
        step3.dependencies = {"step_2"};
        step3.resolved = false;

        plan.steps = {
            step1,
            step2,
            step3
        };

        auto result =
            resolver.resolve(plan);

        printResult(
            "LINEAR_DEPENDENCIES",
            result
        );

        bool correct =
            result.valid &&
            result.executionOrder.size() == 3 &&
            result.executionOrder[0] == "step_1" &&
            result.executionOrder[1] == "step_2" &&
            result.executionOrder[2] == "step_3";

        if (correct)
        {
            passed++;
        }
    }

    // --------------------------------------------------
    // TEST 2: Independent steps
    // --------------------------------------------------

    {
        total++;

        MultiStepPlan plan =
            makeBasePlan();

        MultiStepAction step1;
        step1.id = "step_1";
        step1.description = "First independent step";
        step1.resolved = false;

        MultiStepAction step2;
        step2.id = "step_2";
        step2.description = "Second independent step";
        step2.resolved = false;

        MultiStepAction step3;
        step3.id = "step_3";
        step3.description = "Third independent step";
        step3.resolved = false;

        plan.steps = {
            step1,
            step2,
            step3
        };

        auto result =
            resolver.resolve(plan);

        printResult(
            "INDEPENDENT_STEPS",
            result
        );

        bool correct =
            result.valid &&
            result.executionOrder.size() == 3;

        if (correct)
        {
            passed++;
        }
    }

    // --------------------------------------------------
    // TEST 3: Unknown dependency
    // --------------------------------------------------

    {
        total++;

        MultiStepPlan plan =
            makeBasePlan();

        MultiStepAction step1;
        step1.id = "step_1";
        step1.description = "Invalid dependency test";
        step1.dependencies = {"step_99"};
        step1.resolved = false;

        plan.steps = {
            step1
        };

        auto result =
            resolver.resolve(plan);

        printResult(
            "UNKNOWN_DEPENDENCY",
            result
        );

        bool correct =
            !result.valid &&
            result.executionOrder.empty();

        if (correct)
        {
            passed++;
        }
    }

    // --------------------------------------------------
    // TEST 4: Self dependency
    // --------------------------------------------------

    {
        total++;

        MultiStepPlan plan =
            makeBasePlan();

        MultiStepAction step1;
        step1.id = "step_1";
        step1.description = "Self dependency test";
        step1.dependencies = {"step_1"};
        step1.resolved = false;

        plan.steps = {
            step1
        };

        auto result =
            resolver.resolve(plan);

        printResult(
            "SELF_DEPENDENCY",
            result
        );

        bool correct =
            !result.valid &&
            result.executionOrder.empty();

        if (correct)
        {
            passed++;
        }
    }

    // --------------------------------------------------
    // TEST 5: Duplicate step IDs
    // --------------------------------------------------

    {
        total++;

        MultiStepPlan plan =
            makeBasePlan();

        MultiStepAction step1;
        step1.id = "step_1";
        step1.description = "First duplicate";
        step1.resolved = false;

        MultiStepAction step2;
        step2.id = "step_1";
        step2.description = "Second duplicate";
        step2.resolved = false;

        plan.steps = {
            step1,
            step2
        };

        auto result =
            resolver.resolve(plan);

        printResult(
            "DUPLICATE_STEP_IDS",
            result
        );

        bool correct =
            !result.valid &&
            result.executionOrder.empty();

        if (correct)
        {
            passed++;
        }
    }

    // --------------------------------------------------
    // TEST 6: Dependency cycle
    // --------------------------------------------------

    {
        total++;

        MultiStepPlan plan =
            makeBasePlan();

        MultiStepAction step1;
        step1.id = "step_1";
        step1.description = "Cycle A";
        step1.dependencies = {"step_3"};
        step1.resolved = false;

        MultiStepAction step2;
        step2.id = "step_2";
        step2.description = "Cycle B";
        step2.dependencies = {"step_1"};
        step2.resolved = false;

        MultiStepAction step3;
        step3.id = "step_3";
        step3.description = "Cycle C";
        step3.dependencies = {"step_2"};
        step3.resolved = false;

        plan.steps = {
            step1,
            step2,
            step3
        };

        auto result =
            resolver.resolve(plan);

        printResult(
            "DEPENDENCY_CYCLE",
            result
        );

        bool correct =
            !result.valid &&
            result.executionOrder.empty();

        if (correct)
        {
            passed++;
        }
    }

    // --------------------------------------------------
    // FINAL RESULT
    // --------------------------------------------------

    cout << endl;
    cout << "============================" << endl;
    cout << "DEPENDENCY RESOLVER TEST" << endl;
    cout << "============================" << endl;

    cout << "Passed: "
         << passed
         << " / "
         << total
         << endl;

    if (passed == total)
    {
        cout << "RESULT: PASS" << endl;
        return 0;
    }

    cout << "RESULT: FAIL" << endl;
    return 1;
}