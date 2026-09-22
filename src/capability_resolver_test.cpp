#include "capability_resolver.hpp"

#include <iostream>

using namespace std;

void runTest(
    CapabilityResolver& resolver,
    const string& actionName
)
{
    Action action;

    action.name = actionName;

    Capability result =
        resolver.resolve(action);

    cout << endl;
    cout << "Action: "
         << action.name
         << endl;

    cout << "Supported: "
         << (result.supported ? "true" : "false")
         << endl;

    cout << "Capability: "
         << (
                result.name.empty()
                ? "[none]"
                : result.name
            )
         << endl;

    cout << "Message: "
         << result.message
         << endl;
}

int main()
{
    CapabilityResolver resolver;

    cout << "============================================================"
         << endl;

    cout << "CAPABILITY RESOLVER TEST"
         << endl;

    cout << "============================================================"
         << endl;

    // TEST 1
    runTest(
        resolver,
        "create_folder"
    );

    // TEST 2
    runTest(
        resolver,
        "open_app"
    );

    // TEST 3
    runTest(
        resolver,
        "close_app"
    );

    // TEST 4 — UNSUPPORTED
    runTest(
        resolver,
        "delete_everything"
    );

    // TEST 5 — EMPTY ACTION
    runTest(
        resolver,
        ""
    );

    return 0;
}