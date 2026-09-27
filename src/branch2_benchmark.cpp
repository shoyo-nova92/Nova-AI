#include "single_action_pipeline.hpp"

#include <iostream>
#include <string>
#include <vector>

using namespace std;

enum class ExpectedOutcome
{
    SUCCESS,
    FAILED,
    NOT_EXECUTED
};

struct BenchmarkTest
{
    string name;
    string input;
    ExpectedOutcome expectedOutcome;
    ExecutionFailureReason expectedReason;
};

void printResult(
    const BenchmarkTest& test,
    const SingleActionResult& result
)
{
    cout << endl;
    cout << "------------------------------------------------------------" << endl;
    cout << "TEST: " << test.name << endl;
    cout << "INPUT: " << test.input << endl;
    cout << "------------------------------------------------------------" << endl;

    cout << "Pipeline Success: "
         << (result.success ? "true" : "false")
         << endl;

    cout << "Action: "
         << result.action.name
         << endl;

    cout << "Capability: "
         << result.capability.name
         << endl;

    cout << "Parameters:" << endl;

    cout << "  Name: "
         << (result.parameters.name.empty()
                ? "[none]"
                : result.parameters.name)
         << endl;

    cout << "  Location: "
         << (result.parameters.location.empty()
                ? "[none]"
                : result.parameters.location)
         << endl;

    cout << "  Application: "
         << (result.parameters.application.empty()
                ? "[none]"
                : result.parameters.application)
         << endl;

    cout << "Plan Valid: "
         << (result.executionPlan.valid ? "true" : "false")
         << endl;

    cout << "Execution Status: "
         << static_cast<int>(
                result.executionResult.status
            )
         << endl;

    cout << "Execution Reason: "
         << static_cast<int>(
                result.executionResult.reason
            )
         << endl;

    cout << "Confidence: "
         << result.executionResult.confidence
         << endl;

    cout << "Target Resolved: "
         << (
                result.executionResult.targetResolved
                ? "true"
                : "false"
            )
         << endl;

    cout << "Execution Mechanism Resolved: "
         << (
                result.executionResult.executionMechanismResolved
                ? "true"
                : "false"
            )
         << endl;

    cout << "Execution Message: "
         << result.executionResult.message
         << endl;

    cout << "Pipeline Message: "
         << result.message
         << endl;
}

bool evaluateResult(
    const BenchmarkTest& test,
    const SingleActionResult& result
)
{
    bool outcomeMatches = false;
    bool reasonMatches = false;

    switch (test.expectedOutcome)
    {
        case ExpectedOutcome::SUCCESS:
            outcomeMatches =
                result.executionResult.status
                == ExecutionStatus::SUCCESS;
            break;

        case ExpectedOutcome::FAILED:
            outcomeMatches =
                result.executionResult.status
                == ExecutionStatus::FAILED;
            break;

        case ExpectedOutcome::NOT_EXECUTED:
            outcomeMatches =
                result.executionResult.status
                == ExecutionStatus::NOT_EXECUTED;
            break;
    }

    reasonMatches =
        result.executionResult.reason
        == test.expectedReason;

    return outcomeMatches && reasonMatches;
}

int main()
{
    cout << endl;
    cout << "============================================================" << endl;
    cout << "NOVA BRANCH 2 BENCHMARK" << endl;
    cout << "============================================================" << endl;

    vector<BenchmarkTest> tests =
    {
        {
            "NORMAL_CREATE_FOLDER",
            "create a folder called NovaBench_Create_Success on my desktop",
            ExpectedOutcome::SUCCESS,
            ExecutionFailureReason::NONE
        },

        {
            "CREATE_FOLDER_EXISTING",
            "create a folder called NovaBench_01 on my desktop",
            ExpectedOutcome::FAILED,
            ExecutionFailureReason::EXECUTION_ERROR
        },
        {
            "NORMAL_CREATE_FOLDER",
            "create a folder called NovaBench_Create_Success on my desktop",
            ExpectedOutcome::SUCCESS,
            ExecutionFailureReason::NONE
        },

        {
            "CREATE_FOLDER_EXISTING",
            "create a folder called NovaBench_Create_Success on my desktop",
            ExpectedOutcome::FAILED,
            ExecutionFailureReason::EXECUTION_ERROR
        },

        {
            "NORMAL_OPEN_CHROME",
            "open chrome",
            ExpectedOutcome::SUCCESS,
            ExecutionFailureReason::NONE
        },

        {
            "NORMAL_OPEN_POWERPOINT",
            "open powerpoint",
            ExpectedOutcome::SUCCESS,
            ExecutionFailureReason::NONE
        },

        {
            "NORMAL_OPEN_VSCODE",
            "open visual studio code",
            ExpectedOutcome::SUCCESS,
            ExecutionFailureReason::NONE
        },

        {
            "MISSING_APPLICATION",
            "open",
            ExpectedOutcome::NOT_EXECUTED,
            ExecutionFailureReason::INVALID_PLAN
        },

        {
            "MISSING_FOLDER_NAME",
            "create a folder",
            ExpectedOutcome::NOT_EXECUTED,
            ExecutionFailureReason::INVALID_PLAN
        },

        {
            "MISSING_FOLDER_LOCATION",
            "create a folder called NovaBench_02",
            ExpectedOutcome::NOT_EXECUTED,
            ExecutionFailureReason::INVALID_PLAN
        },

        {
            "SPACES_IN_FOLDER_NAME",
            "create a folder called NovaBench_Spaces_Success on my desktop",
            ExpectedOutcome::SUCCESS,
            ExecutionFailureReason::NONE
        },

        {
            "INVALID_WINDOWS_NAME",
            "create a folder called Test<>Folder on my desktop",
            ExpectedOutcome::FAILED,
            ExecutionFailureReason::EXECUTION_ERROR
        },

        {
            "UNSUPPORTED_LOCATION",
            "create a folder called NovaBench_03 in downloads",
            ExpectedOutcome::NOT_EXECUTED,
            ExecutionFailureReason::INVALID_PLAN
        },

        {
            "NONEXISTENT_APPLICATION",
            "open SomeApplicationThatDoesNotExist_928374",
            ExpectedOutcome::NOT_EXECUTED,
            ExecutionFailureReason::TARGET_NOT_FOUND
        },

        {
            "POLITE_OPEN",
            "hey Nova can you please open chrome",
            ExpectedOutcome::NOT_EXECUTED,
            ExecutionFailureReason::INVALID_PLAN
        },

        {
            "POLITE_CREATE_FOLDER",
            "could you create a folder called NovaBench_Polite_Success on my desktop please",
            ExpectedOutcome::SUCCESS,
            ExecutionFailureReason::NONE
        },

        {
            "MULTI_ACTION",
            "open chrome and create a folder called NovaBench_05",
            ExpectedOutcome::NOT_EXECUTED,
            ExecutionFailureReason::INVALID_PLAN
        },

        {
            "UNSUPPORTED_CLOSE",
            "close spotify",
            ExpectedOutcome::NOT_EXECUTED,
            ExecutionFailureReason::UNSUPPORTED_ACTION
        },

        {
            "UNSUPPORTED_DELETE",
            "delete everything",
            ExpectedOutcome::NOT_EXECUTED,
            ExecutionFailureReason::INVALID_PLAN
        },

        {
            "UNSUPPORTED_RESTART",
            "restart chrome",
            ExpectedOutcome::NOT_EXECUTED,
            ExecutionFailureReason::INVALID_PLAN
        },

        {
            "AMBIGUOUS_ACTION",
            "do the thing I asked earlier",
            ExpectedOutcome::NOT_EXECUTED,
            ExecutionFailureReason::INVALID_PLAN
        }
    };

    SingleActionPipeline pipeline;

    int passed = 0;
    int failed = 0;

    for (const auto& test : tests)
    {
        SingleActionResult result =
            pipeline.process(test.input);

        printResult(
            test,
            result
        );

        bool testPassed =
            evaluateResult(
                test,
                result
            );

        if (testPassed)
        {
            cout << "[RESULT] PASS" << endl;
            passed++;
        }
        else
        {
            cout << "[RESULT] FAIL" << endl;

            cout << "Expected Outcome: "
                 << static_cast<int>(
                        test.expectedOutcome
                    )
                 << endl;

            cout << "Actual Outcome: "
                 << static_cast<int>(
                        result.executionResult.status
                    )
                 << endl;

            cout << "Expected Reason: "
                 << static_cast<int>(
                        test.expectedReason
                    )
                 << endl;

            cout << "Actual Reason: "
                 << static_cast<int>(
                        result.executionResult.reason
                    )
                 << endl;

            failed++;
        }

        cout << endl;
        cout << "[BENCHMARK] Test completed."
             << endl;
    }

    cout << endl;
    cout << "============================================================" << endl;
    cout << "BRANCH 2 BENCHMARK SUMMARY" << endl;
    cout << "============================================================" << endl;

    cout << "Total Tests: "
         << tests.size()
         << endl;

    cout << "Passed: "
         << passed
         << endl;

    cout << "Failed: "
         << failed
         << endl;

    cout << "============================================================" << endl;

    if (failed == 0)
    {
        cout << "BRANCH 2 BENCHMARK: PASS"
             << endl;
    }
    else
    {
        cout << "BRANCH 2 BENCHMARK: FAIL"
             << endl;
    }

    cout << "============================================================" << endl;

    return failed == 0 ? 0 : 1;
}