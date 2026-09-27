#include "capability_resolver.hpp"

#include <iostream>
#include <vector>

using namespace std;

bool testAction(
    CapabilityResolver& resolver,
    const string& actionName,
    const string& expectedCapability
)
{
    Action action;
    action.name = actionName;

    Capability result =
        resolver.resolve(action);

    bool passed =
        result.supported &&
        result.name == expectedCapability;

    cout << "["
         << (passed ? "PASS" : "FAIL")
         << "] Action: "
         << actionName
         << " -> "
         << (
                result.name.empty()
                ? "[none]"
                : result.name
            )
         << endl;

    return passed;
}

bool testPrimitive(
    CapabilityResolver& resolver,
    const string& primitiveName,
    const string& expectedCapability
)
{
    PrimitiveOperation operation;

    operation.name = primitiveName;
    operation.resolved = true;

    Capability result =
        resolver.resolve(operation);

    bool passed =
        result.supported &&
        result.name == expectedCapability;

    cout << "["
         << (passed ? "PASS" : "FAIL")
         << "] Primitive: "
         << primitiveName
         << " -> "
         << (
                result.name.empty()
                ? "[none]"
                : result.name
            )
         << endl;

    return passed;
}

int main()
{
    CapabilityResolver resolver;

    int passed = 0;
    int total = 0;

    cout << "============================================================"
         << endl;

    cout << "CAPABILITY RESOLVER TEST"
         << endl;

    cout << "============================================================"
         << endl;

    // ========================================================
    // BRANCH 2 — EXISTING ACTION RESOLUTION
    // ========================================================

    cout << endl;
    cout << "BRANCH 2 ACTION TESTS"
         << endl;

    total++;
    if (
        testAction(
            resolver,
            "create_folder",
            "filesystem"
        )
    )
    {
        passed++;
    }

    total++;
    if (
        testAction(
            resolver,
            "open_app",
            "application_control"
        )
    )
    {
        passed++;
    }

    total++;
    if (
        testAction(
            resolver,
            "close_app",
            "application_control"
        )
    )
    {
        passed++;
    }

    // ========================================================
    // B3 — PRIMITIVE RESOLUTION
    // ========================================================

    cout << endl;
    cout << "B3 PRIMITIVE TESTS"
         << endl;

    vector<pair<string, string>> primitives =
    {
        {
            "filesystem.create",
            "filesystem"
        },
        {
            "filesystem.create_file",
            "filesystem"
        },
        {
            "filesystem.delete",
            "filesystem"
        },
        {
            "filesystem.move",
            "filesystem"
        },
        {
            "filesystem.copy",
            "filesystem"
        },
        {
            "filesystem.rename",
            "filesystem"
        },
        {
            "application.launch",
            "application_control"
        },
        {
            "application.close",
            "application_control"
        },
        {
            "application.focus",
            "application_control"
        },
        {
            "text.write",
            "text_input"
        },
        {
            "keyboard.press",
            "keyboard_control"
        },
        {
            "keyboard.type",
            "keyboard_control"
        },
        {
            "mouse.click",
            "mouse_control"
        },
        {
            "mouse.move",
            "mouse_control"
        }
    };

    for (
        const auto& primitive :
        primitives
    )
    {
        total++;

        if (
            testPrimitive(
                resolver,
                primitive.first,
                primitive.second
            )
        )
        {
            passed++;
        }
    }

    // ========================================================
    // NEGATIVE TESTS
    // ========================================================

    cout << endl;
    cout << "NEGATIVE TESTS"
         << endl;

    total++;

    Action unsupportedAction;
    unsupportedAction.name =
        "delete_everything";

    Capability actionResult =
        resolver.resolve(
            unsupportedAction
        );

    bool unsupportedActionPassed =
        !actionResult.supported;

    cout << "["
         << (
                unsupportedActionPassed
                ? "PASS"
                : "FAIL"
            )
         << "] Unsupported action"
         << endl;

    if (unsupportedActionPassed)
    {
        passed++;
    }

    total++;

    PrimitiveOperation unsupportedPrimitive;
    unsupportedPrimitive.name =
        "unknown.primitive";

    Capability primitiveResult =
        resolver.resolve(
            unsupportedPrimitive
        );

    bool unsupportedPrimitivePassed =
        !primitiveResult.supported;

    cout << "["
         << (
                unsupportedPrimitivePassed
                ? "PASS"
                : "FAIL"
            )
         << "] Unsupported primitive"
         << endl;

    if (unsupportedPrimitivePassed)
    {
        passed++;
    }

    // ========================================================
    // FINAL RESULT
    // ========================================================

    cout << endl;
    cout << "============================================================"
         << endl;

    cout << "Passed: "
         << passed
         << " / "
         << total
         << endl;

    cout << "RESULT: "
         << (
                passed == total
                ? "PASS"
                : "FAIL"
            )
         << endl;

    cout << "============================================================"
         << endl;

    return (
        passed == total
        ? 0
        : 1
    );
}