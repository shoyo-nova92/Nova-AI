#include "action_primitive_resolver.hpp"

#include <iostream>

using namespace std;

void printResult(
    const string& name,
    const ActionPrimitiveResolutionResult& result
)
{
    cout << endl;
    cout << "[" << name << "]" << endl;

    cout << "Status: ";

    switch (result.status)
    {
        case ActionPrimitiveResolutionStatus::RESOLVED:
            cout << "RESOLVED";
            break;

        case ActionPrimitiveResolutionStatus::CLARIFICATION_REQUIRED:
            cout << "CLARIFICATION_REQUIRED";
            break;

        case ActionPrimitiveResolutionStatus::FAILED:
            cout << "FAILED";
            break;
    }

    cout << endl;

    cout << "Message: "
         << result.message
         << endl;

    if (!result.clarificationQuestion.empty())
    {
        cout << "Clarification: "
             << result.clarificationQuestion
             << endl;
    }

    cout << "Operation Count: "
         << result.action.operations.size()
         << endl;

    for (const auto& operation :
         result.action.operations)
    {
        cout << "Primitive: "
             << operation.name
             << endl;

        cout << "Resolved: "
             << (operation.resolved ? "true" : "false")
             << endl;
    }
}

MultiStepAction makeAction(
    const string& id,
    const string& description
)
{
    MultiStepAction action;

    action.id = id;
    action.description = description;
    action.dependencies.clear();
    action.operations.clear();
    action.operation = "";
    action.capability = "";
    action.parameters.clear();
    action.resolved = false;

    return action;
}

int main()
{
    ActionPrimitiveResolver resolver;

    int passed = 0;
    int total = 0;

    // --------------------------------------------------
    // TEST 1: Deterministic folder creation
    // --------------------------------------------------

    {
        total++;

        auto action =
            makeAction(
                "step_1",
                "Create a folder on the desktop"
            );

        auto result =
            resolver.resolve(action);

        printResult(
            "CREATE_FOLDER",
            result
        );

        bool correct =
            result.status ==
                ActionPrimitiveResolutionStatus::RESOLVED
            &&
            result.action.operations.size() == 1
            &&
            result.action.operations[0].name ==
                "filesystem.create";

        if (correct)
        {
            passed++;
        }
    }

    // --------------------------------------------------
    // TEST 2: Deterministic Java file creation
    // --------------------------------------------------

    {
        total++;

        auto action =
            makeAction(
                "step_2",
                "Create a basic Main.java class file"
            );

        auto result =
            resolver.resolve(action);

        printResult(
            "CREATE_JAVA_FILE",
            result
        );

        bool correct =
            result.status ==
                ActionPrimitiveResolutionStatus::RESOLVED
            &&
            result.action.operations.size() == 1
            &&
            result.action.operations[0].name ==
                "filesystem.create_file";

        if (correct)
        {
            passed++;
        }
    }

    // --------------------------------------------------
    // TEST 3: Deterministic application launch
    // --------------------------------------------------

    {
        total++;

        auto action =
            makeAction(
                "step_3",
                "Open the project in VS Code"
            );

        auto result =
            resolver.resolve(action);

        printResult(
            "OPEN_APPLICATION",
            result
        );

        bool correct =
            result.status ==
                ActionPrimitiveResolutionStatus::RESOLVED
            &&
            result.action.operations.size() == 1
            &&
            result.action.operations[0].name ==
                "application.launch";

        if (correct)
        {
            passed++;
        }
    }

    // --------------------------------------------------
    // TEST 4: Deterministic text writing
    // --------------------------------------------------

    {
        total++;

        auto action =
            makeAction(
                "step_4",
                "Write the basic Java class into the file"
            );

        auto result =
            resolver.resolve(action);

        printResult(
            "WRITE_TEXT",
            result
        );

        bool correct =
            result.status ==
                ActionPrimitiveResolutionStatus::RESOLVED
            &&
            result.action.operations.size() == 1
            &&
            result.action.operations[0].name ==
                "text.write";

        if (correct)
        {
            passed++;
        }
    }

    // --------------------------------------------------
    // TEST 5: Ambiguous semantic step
    //
    // The LLM should NOT invent a plan.
    // It should request clarification.
    // --------------------------------------------------

    {
        total++;

        auto action =
            makeAction(
                "step_5",
                "Prepare the project for Java development"
            );

        auto result =
            resolver.resolve(action);

        printResult(
            "AMBIGUOUS_SEMANTIC_STEP",
            result
        );

        bool correct =
            result.status ==
                ActionPrimitiveResolutionStatus::CLARIFICATION_REQUIRED
            &&
            !result.clarificationQuestion.empty()
            &&
            result.action.operations.empty();

        if (correct)
        {
            passed++;
        }
    }

    // --------------------------------------------------
    // TEST 6: Empty description
    // --------------------------------------------------

    {
        total++;

        auto action =
            makeAction(
                "step_6",
                ""
            );

        auto result =
            resolver.resolve(action);

        printResult(
            "EMPTY_DESCRIPTION",
            result
        );

        bool correct =
            result.status ==
                ActionPrimitiveResolutionStatus::FAILED
            &&
            result.action.operations.empty();

        if (correct)
        {
            passed++;
        }
    }

    // --------------------------------------------------
    // FINAL RESULT
    // --------------------------------------------------

    cout << endl;
    cout << "==================================" << endl;
    cout << "ACTION / PRIMITIVE RESOLVER TEST" << endl;
    cout << "==================================" << endl;

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