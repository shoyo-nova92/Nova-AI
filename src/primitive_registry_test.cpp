#include "primitive_registry.hpp"

#include <iostream>

using namespace std;

int main()
{
    PrimitiveOperationType type;

    int passed = 0;
    int total = 0;

    // --------------------------------------------------
    // TEST 1: Known filesystem primitive
    // --------------------------------------------------

    total++;

    bool createSupported =
        PrimitiveRegistry::isSupported(
            "filesystem.create"
        );

    bool createResolved =
        PrimitiveRegistry::resolveType(
            "filesystem.create",
            type
        );

    bool test1 =
        createSupported &&
        createResolved &&
        type ==
            PrimitiveOperationType::FILESYSTEM_CREATE;

    cout << "[FILESYSTEM_CREATE] "
         << (test1 ? "PASS" : "FAIL")
         << endl;

    if (test1)
    {
        passed++;
    }

    // --------------------------------------------------
    // TEST 2: Known application primitive
    // --------------------------------------------------

    total++;

    bool launchSupported =
        PrimitiveRegistry::isSupported(
            "application.launch"
        );

    bool launchResolved =
        PrimitiveRegistry::resolveType(
            "application.launch",
            type
        );

    bool test2 =
        launchSupported &&
        launchResolved &&
        type ==
            PrimitiveOperationType::APPLICATION_LAUNCH;

    cout << "[APPLICATION_LAUNCH] "
         << (test2 ? "PASS" : "FAIL")
         << endl;

    if (test2)
    {
        passed++;
    }

    // --------------------------------------------------
    // TEST 3: Known keyboard primitive
    // --------------------------------------------------

    total++;

    bool keyboardSupported =
        PrimitiveRegistry::isSupported(
            "keyboard.type"
        );

    bool keyboardResolved =
        PrimitiveRegistry::resolveType(
            "keyboard.type",
            type
        );

    bool test3 =
        keyboardSupported &&
        keyboardResolved &&
        type ==
            PrimitiveOperationType::KEYBOARD_TYPE;

    cout << "[KEYBOARD_TYPE] "
         << (test3 ? "PASS" : "FAIL")
         << endl;

    if (test3)
    {
        passed++;
    }

    // --------------------------------------------------
    // TEST 4: Unknown primitive
    // --------------------------------------------------

    total++;

    bool unknownSupported =
        PrimitiveRegistry::isSupported(
            "system.destroy_everything"
        );

    bool unknownResolved =
        PrimitiveRegistry::resolveType(
            "system.destroy_everything",
            type
        );

    bool test4 =
        !unknownSupported &&
        !unknownResolved;

    cout << "[UNKNOWN_PRIMITIVE] "
         << (test4 ? "PASS" : "FAIL")
         << endl;

    if (test4)
    {
        passed++;
    }

    // --------------------------------------------------
    // TEST 5: Supported primitive list
    // --------------------------------------------------

    total++;

    vector<string> primitives =
        PrimitiveRegistry::getSupportedNames();

    bool test5 =
        !primitives.empty();

    cout << "[REGISTRY_NOT_EMPTY] "
         << (test5 ? "PASS" : "FAIL")
         << endl;

    if (test5)
    {
        passed++;
    }

    // --------------------------------------------------
    // TEST 6: Defined primitive parameter schema
    // --------------------------------------------------

    total++;

    PrimitiveDefinition createFileDefinition{};
    bool createFileDefinitionResolved =
        PrimitiveRegistry::resolveDefinition(
            "filesystem.create_file",
            createFileDefinition
        );

    bool test6 =
        createFileDefinitionResolved &&
        createFileDefinition.parameters.has_value() &&
        createFileDefinition.parameters->name ==
            ParameterRequirement::REQUIRED &&
        createFileDefinition.parameters->location ==
            ParameterRequirement::REQUIRED &&
        createFileDefinition.parameters->content ==
            ParameterRequirement::OPTIONAL;

    cout << "[CREATE_FILE_SCHEMA] "
         << (test6 ? "PASS" : "FAIL")
         << endl;

    if (test6)
    {
        passed++;
    }

    // --------------------------------------------------
    // TEST 7: Undesigned schema remains undefined
    // --------------------------------------------------

    total++;

    PrimitiveDefinition deleteDefinition{};
    bool deleteDefinitionResolved =
        PrimitiveRegistry::resolveDefinition(
            "filesystem.delete",
            deleteDefinition
        );

    bool test7 =
        deleteDefinitionResolved &&
        !deleteDefinition.parameters.has_value();

    cout << "[UNDEFINED_SCHEMA] "
         << (test7 ? "PASS" : "FAIL")
         << endl;

    if (test7)
    {
        passed++;
    }

    // --------------------------------------------------
    // FINAL RESULT
    // --------------------------------------------------

    cout << endl;
    cout << "============================" << endl;
    cout << "PRIMITIVE REGISTRY TEST" << endl;
    cout << "============================" << endl;

    cout << "Registered primitives: "
         << primitives.size()
         << endl;

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