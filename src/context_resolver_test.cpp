#include "context_resolver.hpp"

#include <iostream>
using namespace std;

int main()
{
    ContextResolver resolver;

    int passed = 0;
    int total = 0;

    // ============================================================
    // TEST 1 — PROJECT DIRECTORY
    // ============================================================

    {
        total++;

        bool result =
            resolver.containsContextReference(
                "create Main.java in the project directory"
            );

        bool passedTest =
            result;

        cout << "["
             << (passedTest ? "PASS" : "FAIL")
             << "] Project directory reference"
             << endl;

        if (passedTest)
        {
            passed++;
        }
    }

    // ============================================================
    // TEST 12 — TYPED CONTEXT REFERENCE TO PRIMITIVE PARAMETERS
    // ============================================================

    {
        total++;

        MultiStepAction sourceStep;
        sourceStep.id = "step_1";

        PrimitiveOperation createFolder;
        createFolder.type = PrimitiveOperationType::FILESYSTEM_CREATE;
        createFolder.name = "filesystem.create";
        createFolder.resolved = true;
        createFolder.parameters.name = "NovaProject";
        createFolder.parameters.location = "desktop";
        sourceStep.operations.push_back(createFolder);

        MultiStepAction dependentStep;
        dependentStep.id = "step_2";
        dependentStep.description = "Create Main.java inside that folder";

        PrimitiveOperation createFile;
        createFile.type = PrimitiveOperationType::FILESYSTEM_CREATE_FILE;
        createFile.name = "filesystem.create_file";
        createFile.resolved = true;
        dependentStep.operations.push_back(createFile);

        ActionParameters semanticParameters;
        semanticParameters.name = ParameterValue{
            ParameterValueType::LITERAL,
            "Main.java",
            ContextParameterReference{}
        };
        semanticParameters.location = ParameterValue{
            ParameterValueType::CONTEXT_REFERENCE,
            "",
            ContextParameterReference{"step_1", "path"}
        };
        dependentStep.semanticParameters.push_back(semanticParameters);

        MultiStepPlan plan;
        plan.valid = true;
        plan.steps = {sourceStep, dependentStep};

        ContextResolutionResult result = resolver.resolve(
            dependentStep,
            plan);

        const PrimitiveOperation& resolvedOperation =
            result.action.operations.front();
        const bool passedTest =
            result.status == ContextResolutionStatus::RESOLVED &&
            resolvedOperation.parameters.name == "Main.java" &&
            resolvedOperation.parameters.location == "desktop/NovaProject" &&
            resolvedOperation.parameters.content.empty() &&
            result.action.contextReferences.size() == 1 &&
            result.action.contextReferences.front().sourceStepId == "step_1";

        cout << endl;
        cout << "["
            << (passedTest ? "PASS" : "FAIL")
            << "] Typed context reference resolves to execution parameters"
            << endl;

        if (passedTest)
        {
            passed++;
        }
    }

    // ============================================================
    // TEST 2 — THE FOLDER
    // ============================================================

    {
        total++;

        bool result =
            resolver.containsContextReference(
                "create a file in the folder"
            );

        bool passedTest =
            result;

        cout << "["
             << (passedTest ? "PASS" : "FAIL")
             << "] Folder reference"
             << endl;

        if (passedTest)
        {
            passed++;
        }
    }

    // ============================================================
    // TEST 3 — THAT FOLDER
    // ============================================================

    {
        total++;

        bool result =
            resolver.containsContextReference(
                "open that folder in VS Code"
            );

        bool passedTest =
            result;

        cout << "["
             << (passedTest ? "PASS" : "FAIL")
             << "] That-folder reference"
             << endl;

        if (passedTest)
        {
            passed++;
        }
    }

    // ============================================================
    // TEST 4 — THE FILE
    // ============================================================

    {
        total++;

        bool result =
            resolver.containsContextReference(
                "open the file"
            );

        bool passedTest =
            result;

        cout << "["
             << (passedTest ? "PASS" : "FAIL")
             << "] File reference"
             << endl;

        if (passedTest)
        {
            passed++;
        }
    }

    // ============================================================
    // TEST 5 — THAT FILE
    // ============================================================

    {
        total++;

        bool result =
            resolver.containsContextReference(
                "edit that file"
            );

        bool passedTest =
            result;

        cout << "["
             << (passedTest ? "PASS" : "FAIL")
             << "] That-file reference"
             << endl;

        if (passedTest)
        {
            passed++;
        }
    }

    // ============================================================
    // TEST 6 — NO CONTEXT
    // ============================================================

    {
        total++;

        bool result =
            resolver.containsContextReference(
                "create Main.java"
            );

        bool passedTest =
            !result;

        cout << "["
             << (passedTest ? "PASS" : "FAIL")
             << "] No context reference"
             << endl;

        if (passedTest)
        {
            passed++;
        }
    }
    // ============================================================
    // TEST 7 — PREVIOUS STEP CONTEXT RESOLUTION
    // ============================================================

    {
        total++;

        MultiStepAction step1;

        step1.id =
            "step_1";

        step1.description =
            "Create a folder called MyProject on my desktop";

        PrimitiveOperation createFolder;

        createFolder.name =
            "filesystem.create";

        createFolder.resolved =
            true;

        createFolder.parameters.name =
            "MyProject";

        createFolder.parameters.location =
            "desktop";

        step1.operations.push_back(
            createFolder
        );


        MultiStepAction step2;

        step2.id =
            "step_2";

        step2.description =
            "Create Main.java in the project directory";

        MultiStepPlan plan;

        plan.valid =
            true;

        plan.goal =
            "Create a Java project";

        plan.steps.push_back(
            step1
        );

        plan.steps.push_back(
            step2
        );


        bool passedTest =
            resolver.resolvePreviousStepContext(
                plan.steps[1],
                plan
            );

        bool referenceFound =
            !plan.steps[1].contextReferences.empty();

        bool referenceResolved =
            referenceFound &&
            plan.steps[1].contextReferences[0].resolved;

        bool correctSource =
            referenceFound &&
            plan.steps[1].contextReferences[0].sourceStepId ==
                "step_1";

        bool correctValue =
            referenceFound &&
            plan.steps[1].contextReferences[0].resolvedValue ==
                "desktop/MyProject";

        bool passedTestFinal =
            passedTest &&
            referenceResolved &&
            correctSource &&
            correctValue;

        cout << endl;

        cout << "["
            << (passedTestFinal ? "PASS" : "FAIL")
            << "] Previous-step context resolution"
            << endl;

        cout << "Resolved: "
            << (referenceResolved ? "true" : "false")
            << endl;

        cout << "Source step: "
            << (
                    referenceFound
                    ? plan.steps[1].contextReferences[0].sourceStepId
                    : "[none]"
                )
            << endl;

        cout << "Resolved value: "
            << (
                    referenceFound
                    ? plan.steps[1].contextReferences[0].resolvedValue
                    : "[none]"
                )
            << endl;

        if (passedTestFinal)
        {
            passed++;
        }
    }
    // ============================================================
    // TEST 8 — UNRESOLVED CONTEXT
    // ============================================================

    {
        total++;

        MultiStepAction step;

        step.id =
            "step_2";

        step.description =
            "Create Main.java in the project directory";

        MultiStepPlan plan;

        plan.valid =
            true;

        plan.goal =
            "Create a Java project";

        plan.steps.push_back(
            step
        );

        bool result =
            resolver.resolvePreviousStepContext(
                plan.steps[0],
                plan
            );

        bool passedTest =
            !result &&
            plan.steps[0].contextReferences.empty();

        cout << endl;

        cout << "["
            << (passedTest ? "PASS" : "FAIL")
            << "] Unresolved context rejected"
            << endl;

        cout << "Resolution result: "
            << (result ? "true" : "false")
            << endl;

        cout << "Context references: "
            << plan.steps[0].contextReferences.size()
            << endl;

        if (passedTest)
        {
            passed++;
        }
    }

    // ============================================================
    // TEST 9 — PUBLIC RESOLVER: NO CONTEXT
    // ============================================================

    {
        total++;

        MultiStepAction action;

        action.id =
            "step_1";

        action.description =
            "Create Main.java";

        MultiStepPlan plan;

        plan.valid =
            true;

        plan.steps.push_back(
            action
        );

        ContextResolutionResult result =
            resolver.resolve(
                action,
                plan
            );

        bool passedTest =
            result.status ==
                ContextResolutionStatus::RESOLVED;

        cout << endl;

        cout << "["
            << (passedTest ? "PASS" : "FAIL")
            << "] Public resolver — no context"
            << endl;

        if (passedTest)
        {
            passed++;
        }
    }

    // ============================================================
    // TEST 10 — PUBLIC RESOLVER: RESOLVED CONTEXT
    // ============================================================

    {
        total++;

        MultiStepAction step1;

        step1.id =
            "step_1";

        step1.description =
            "Create a folder called MyProject on my desktop";

        PrimitiveOperation createFolder;

        createFolder.name =
            "filesystem.create";

        createFolder.resolved =
            true;

        createFolder.parameters.name =
            "MyProject";

        createFolder.parameters.location =
            "desktop";

        step1.operations.push_back(
            createFolder
        );


        MultiStepAction step2;

        step2.id =
            "step_2";

        step2.description =
            "Create Main.java in the project directory";


        MultiStepPlan plan;

        plan.valid =
            true;

        plan.steps.push_back(
            step1
        );

        plan.steps.push_back(
            step2
        );


        ContextResolutionResult result =
            resolver.resolve(
                step2,
                plan
            );

        bool passedTest =
            result.status ==
                ContextResolutionStatus::RESOLVED &&
            !result.resolvedContext.empty() &&
            result.resolvedContext[
                "Create Main.java in the project directory"
            ] ==
                "desktop/MyProject";

        cout << endl;

        cout << "["
            << (passedTest ? "PASS" : "FAIL")
            << "] Public resolver — context resolved"
            << endl;

        cout << "Status: "
            << (
                    result.status ==
                        ContextResolutionStatus::RESOLVED
                    ? "RESOLVED"
                    : "OTHER"
                )
            << endl;

        if (passedTest)
        {
            passed++;
        }
    }
    // ============================================================
    // TEST 11 — PUBLIC RESOLVER: UNRESOLVED CONTEXT
    // ============================================================

    {
        total++;

        MultiStepAction action;

        action.id =
            "step_2";

        action.description =
            "Create Main.java in the project directory";

        MultiStepPlan plan;

        plan.valid =
            true;

        plan.steps.push_back(
            action
        );

        ContextResolutionResult result =
            resolver.resolve(
                action,
                plan
            );

        bool passedTest =
            result.status ==
                ContextResolutionStatus::CLARIFICATION_REQUIRED &&
            !result.clarificationQuestion.empty();

        cout << endl;

        cout << "["
            << (passedTest ? "PASS" : "FAIL")
            << "] Public resolver — unresolved context"
            << endl;

        cout << "Clarification: "
            << result.clarificationQuestion
            << endl;

        if (passedTest)
        {
            passed++;
        }
    }

    // ============================================================
    // SUMMARY
    // ============================================================

    cout << endl;
    cout << "========================================"
         << endl;

    cout << "CONTEXT REFERENCE DETECTOR TEST"
         << endl;

    cout << "========================================"
         << endl;

    cout << "Passed: "
         << passed
         << " / "
         << total
         << endl;

    cout << "RESULT: "
         << (passed == total ? "PASS" : "FAIL")
         << endl;

    return passed == total ? 0 : 1;
}

