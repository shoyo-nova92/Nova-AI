#include "parameter_resolver.hpp"

#include <iostream>

using namespace std;

void runTest(
    ParameterResolver& resolver,
    const string& input,
    const string& actionName,
    const string& capabilityName
)
{
    Action action;
    action.name = actionName;

    Capability capability;
    capability.supported = true;
    capability.name = capabilityName;
    capability.message = "Test capability.";

    ParameterResolverResult result =
        resolver.resolve(
            input,
            action,
            capability
        );

    cout << endl;
    cout << "Input: "
         << input
         << endl;

    cout << "Action: "
         << action.name
         << endl;

    cout << "Capability: "
         << capability.name
         << endl;

    cout << "Success: "
         << (result.success ? "true" : "false")
         << endl;

    cout << "Name: "
         << (
                result.parameters.name.empty()
                ? "[none]"
                : result.parameters.name
            )
         << endl;

    cout << "Location: "
         << (
                result.parameters.location.empty()
                ? "[none]"
                : result.parameters.location
            )
         << endl;

    cout << "Application: "
         << (
                result.parameters.application.empty()
                ? "[none]"
                : result.parameters.application
            )
         << endl;

    cout << "Message: "
         << result.message
         << endl;
}

int main()
{
    ParameterResolver resolver;

    cout << "============================================================"
         << endl;

    cout << "PARAMETER RESOLVER TEST"
         << endl;

    cout << "============================================================"
         << endl;

    // TEST 1 — CREATE FOLDER
    runTest(
        resolver,
        "create a folder called nova on my desktop",
        "create_folder",
        "filesystem"
    );

    // TEST 2 — OPEN APPLICATION
    runTest(
        resolver,
        "open chrome",
        "open_app",
        "application_control"
    );

    // TEST 3 — LAUNCH APPLICATION
    runTest(
        resolver,
        "launch vscode",
        "open_app",
        "application_control"
    );

    // TEST 4 — CLOSE APPLICATION
    runTest(
        resolver,
        "close spotify",
        "close_app",
        "application_control"
    );

    // TEST 5 — MISSING APPLICATION
    runTest(
        resolver,
        "open ",
        "open_app",
        "application_control"
    );

    // TEST 6 — EMPTY INPUT
    runTest(
        resolver,
        "",
        "open_app",
        "application_control"
    );
        // ========================================================
        // B3 — PRIMITIVE PARAMETER TESTS
        // ========================================================

        cout << endl;
        cout << "============================================================"
            << endl;

        cout << "B3 PRIMITIVE PARAMETER TESTS"
            << endl;

        cout << "============================================================"
            << endl;

        // TEST 7 — CREATE FOLDER PRIMITIVE
        {
            PrimitiveOperation operation;

            operation.name =
                "filesystem.create";

            operation.resolved =
                true;

            Capability capability;

            capability.supported =
                true;

            capability.name =
                "filesystem";

            capability.message =
                "Test capability.";

            ParameterResolverResult result =
                resolver.resolve(
                    "create a folder called Nova on my desktop",
                    operation,
                    capability
                );

            cout << endl;
            cout << "Primitive: "
                << operation.name
                << endl;

            cout << "Success: "
                << (result.success ? "true" : "false")
                << endl;

            cout << "Name: "
                << (
                        result.parameters.name.empty()
                        ? "[none]"
                        : result.parameters.name
                    )
                << endl;

            cout << "Location: "
                << (
                        result.parameters.location.empty()
                        ? "[none]"
                        : result.parameters.location
                    )
                << endl;

            cout << "Message: "
                << result.message
                << endl;
        }
        // TEST 8 — APPLICATION LAUNCH PRIMITIVE
        {
            PrimitiveOperation operation;

            operation.name =
                "application.launch";

            operation.resolved =
                true;

            Capability capability;

            capability.supported =
                true;

            capability.name =
                "application_control";

            capability.message =
                "Test capability.";

            ParameterResolverResult result =
                resolver.resolve(
                    "open vscode",
                    operation,
                    capability
                );

            cout << endl;
            cout << "Primitive: "
                << operation.name
                << endl;

            cout << "Success: "
                << (result.success ? "true" : "false")
                << endl;

            cout << "Application: "
                << (
                        result.parameters.application.empty()
                        ? "[none]"
                        : result.parameters.application
                    )
                << endl;

            cout << "Message: "
                << result.message
                << endl;
        }

        // TEST 9 — CREATE FILE PRIMITIVE
        {
            PrimitiveOperation operation;

            operation.name =
                "filesystem.create_file";

            operation.resolved =
                true;

            Capability capability;

            capability.supported =
                true;

            capability.name =
                "filesystem";

            capability.message =
                "Test capability.";

            ParameterResolverResult result =
                resolver.resolve(
                    "create a Main.java file",
                    operation,
                    capability
                );

            bool passed =
                result.success &&
                result.parameters.name == "Main.java";

            cout << endl;
            cout << "["
                << (passed ? "PASS" : "FAIL")
                << "] Primitive: "
                << operation.name
                << endl;

            cout << "Success: "
                << (result.success ? "true" : "false")
                << endl;

            cout << "Name: "
                << (
                        result.parameters.name.empty()
                        ? "[none]"
                        : result.parameters.name
                    )
                << endl;

            cout << "Location: "
                << (
                        result.parameters.location.empty()
                        ? "[none]"
                        : result.parameters.location
                    )
                << endl;

            cout << "Message: "
                << result.message
                << endl;
        }
        // ============================================================
        // PARAMETER CONVERSION TEST
        // ============================================================

        ActionParameters actionParameters;

        actionParameters.name =
            "Main.java";

        actionParameters.location =
            "desktop";

        actionParameters.application =
            "vscode";

        actionParameters.target =
            "Main.java";

        actionParameters.content =
            "public class Main {}";

        PrimitiveParameters primitiveParameters =
            resolver.toPrimitiveParameters(
                actionParameters
            );

        bool conversionPassed =
            primitiveParameters.name == "Main.java" &&
            primitiveParameters.location == "desktop" &&
            primitiveParameters.application == "vscode" &&
            primitiveParameters.target == "Main.java" &&
            primitiveParameters.content == "public class Main {}";

        if (conversionPassed)
        {
            cout << "[PASS] ActionParameters -> PrimitiveParameters conversion"
                << endl;
        }
        else
        {
            cout << "[FAIL] ActionParameters -> PrimitiveParameters conversion"
                << endl;
        }

        // TEST 10 — CREATE FILE WITHOUT NAME
        {
            PrimitiveOperation operation;

            operation.name =
                "filesystem.create_file";

            operation.resolved =
                true;

            Capability capability;

            capability.supported =
                true;

            capability.name =
                "filesystem";

            capability.message =
                "Test capability.";

            ParameterResolverResult result =
                resolver.resolve(
                    "create a file",
                    operation,
                    capability
                );

            bool passed =
                !result.success;

            cout << endl;
            cout << "["
                << (passed ? "PASS" : "FAIL")
                << "] Missing file name rejected"
                << endl;

            cout << "Message: "
                << result.message
                << endl;
        }
        // TEST 11 — TEXT WRITE
        {
            PrimitiveOperation operation;

            operation.name =
                "text.write";

            operation.resolved =
                true;

            Capability capability;

            capability.supported =
                true;

            capability.name =
                "text_input";

            capability.message =
                "Test capability.";

            ParameterResolverResult result =
                resolver.resolve(
                    "write \"public class Main {}\" into Main.java",
                    operation,
                    capability
                );

            bool passed =
                result.success &&
                result.parameters.target == "Main.java" &&
                result.parameters.content == "public class Main {}";

            cout << endl;
            cout << "["
                << (passed ? "PASS" : "FAIL")
                << "] Primitive: "
                << operation.name
                << endl;

            cout << "Success: "
                << (result.success ? "true" : "false")
                << endl;

            cout << "Target: "
                << (
                        result.parameters.target.empty()
                        ? "[none]"
                        : result.parameters.target
                    )
                << endl;

            cout << "Content: "
                << (
                        result.parameters.content.empty()
                        ? "[none]"
                        : result.parameters.content
                    )
                << endl;

            cout << "Message: "
                << result.message
                << endl;
        }
        // ============================================================
        // MULTI-PRIMITIVE PARAMETER TEST
        // ============================================================

        {
            PrimitiveOperation createFile;

            createFile.name =
                "filesystem.create_file";

            createFile.resolved =
                true;

            Capability filesystemCapability;

            filesystemCapability.supported =
                true;

            filesystemCapability.name =
                "filesystem";

            filesystemCapability.message =
                "Test capability.";

            PrimitiveOperation writeText;

            writeText.name =
                "text.write";

            writeText.resolved =
                true;

            Capability textCapability;

            textCapability.supported =
                true;

            textCapability.name =
                "text_input";

            textCapability.message =
                "Test capability.";

            bool createSuccess =
                resolver.resolveIntoOperation(
                    "create a Main.java file",
                    createFile,
                    filesystemCapability
                );

            bool writeSuccess =
                resolver.resolveIntoOperation(
                    "write \"public class Main {}\" into Main.java",
                    writeText,
                    textCapability
                );

            bool passed =
                createSuccess &&
                writeSuccess &&
                createFile.parameters.name == "Main.java" &&
                writeText.parameters.target == "Main.java" &&
                writeText.parameters.content == "public class Main {}";

            cout << endl;

            cout << "["
                << (passed ? "PASS" : "FAIL")
                << "] Multi-primitive parameter isolation"
                << endl;

            cout << "Create file: "
                << (createSuccess ? "true" : "false")
                << endl;

            cout << "Create file name: "
                << createFile.parameters.name
                << endl;

            cout << "Write text: "
                << (writeSuccess ? "true" : "false")
                << endl;

            cout << "Write target: "
                << writeText.parameters.target
                << endl;

            cout << "Write content: "
                << writeText.parameters.content
                << endl;
        }
        // ============================================================
        // OPERATION PARAMETER INTEGRATION TEST
        // ============================================================

        {
            PrimitiveOperation operation;

            operation.name =
                "filesystem.create_file";

            operation.resolved =
                true;

            Capability capability;

            capability.supported =
                true;

            capability.name =
                "filesystem";

            capability.message =
                "Test capability.";

            bool success =
                resolver.resolveIntoOperation(
                    "create a Main.java file",
                    operation,
                    capability
                );

            bool passed =
                success &&
                operation.parameters.name == "Main.java" &&
                operation.parameters.location.empty() &&
                operation.parameters.application.empty() &&
                operation.parameters.target.empty() &&
                operation.parameters.content.empty();

            cout << endl;

            cout << "["
                << (passed ? "PASS" : "FAIL")
                << "] Parameters attached to PrimitiveOperation"
                << endl;

            cout << "Success: "
                << (success ? "true" : "false")
                << endl;

            cout << "Operation: "
                << operation.name
                << endl;

            cout << "Name: "
                << (
                        operation.parameters.name.empty()
                        ? "[none]"
                        : operation.parameters.name
                    )
                << endl;
        }
    return 0;
}