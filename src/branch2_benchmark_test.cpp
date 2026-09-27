#include "single_action_pipeline.hpp"

#include <filesystem>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

using namespace std;

struct BenchmarkCase
{
    string name;
    string input;
    bool expectSuccess;
    ExecutionStatus expectedStatus;
    ExecutionFailureReason expectedReason;
};

static string statusToString(ExecutionStatus status)
{
    switch (status)
    {
        case ExecutionStatus::SUCCESS:
            return "SUCCESS";
        case ExecutionStatus::FAILED:
            return "FAILED";
        case ExecutionStatus::NOT_EXECUTED:
            return "NOT_EXECUTED";
    }

    return "UNKNOWN";
}

static string reasonToString(ExecutionFailureReason reason)
{
    switch (reason)
    {
        case ExecutionFailureReason::NONE:
            return "NONE";
        case ExecutionFailureReason::INVALID_PLAN:
            return "INVALID_PLAN";
        case ExecutionFailureReason::LOW_CONFIDENCE:
            return "LOW_CONFIDENCE";
        case ExecutionFailureReason::UNSUPPORTED_ACTION:
            return "UNSUPPORTED_ACTION";
        case ExecutionFailureReason::TARGET_NOT_FOUND:
            return "TARGET_NOT_FOUND";
        case ExecutionFailureReason::EXECUTION_ERROR:
            return "EXECUTION_ERROR";
    }

    return "UNKNOWN";
}

static bool matches(
    const SingleActionResult& result,
    const BenchmarkCase& test
)
{
    if (result.executionResult.status != test.expectedStatus)
        return false;

    if (result.executionResult.reason != test.expectedReason)
        return false;

    if (result.success != test.expectSuccess)
        return false;

    return true;
}

int main()
{
    cout << "============================================================\n";
    cout << "NOVA BRANCH 2 ADVERSARIAL BENCHMARK\n";
    cout << "============================================================\n\n";

    /*
        IMPORTANT:
        This benchmark tests Branch 2 directly.

        It intentionally does NOT start Part A, Whisper, wake-word
        detection, the root LLM router, or the V-key.

        That makes failures attributable to Branch 2 itself.
    */

    const string folderA = "NovaB2Bench_Create_A";
    const string folderB = "NovaB2Bench_Create_B";

    /*
        Existing-folder tests require the folders to exist before
        the benchmark starts. We create them here so the benchmark
        is repeatable.
    */
    const filesystem::path desktop =
        filesystem::path(getenv("USERPROFILE"))
        / "OneDrive"
        / "Desktop";

    filesystem::create_directories(desktop / folderA);
    filesystem::create_directories(desktop / folderB);

    vector<BenchmarkCase> tests =
    {
        {
            "01 - Create new folder",
            "create a folder called NovaB2Bench_New on my desktop",
            true,
            ExecutionStatus::SUCCESS,
            ExecutionFailureReason::NONE
        },

        {
            "02 - Create existing folder",
            "create a folder called " + folderA + " on my desktop",
            false,
            ExecutionStatus::FAILED,
            ExecutionFailureReason::EXECUTION_ERROR
        },

        {
            "03 - Create folder with spaces",
            "create a folder called Nova B2 Benchmark Folder on my desktop",
            true,
            ExecutionStatus::SUCCESS,
            ExecutionFailureReason::NONE
        },

        {
            "04 - Missing folder name",
            "create a folder on my desktop",
            false,
            ExecutionStatus::NOT_EXECUTED,
            ExecutionFailureReason::INVALID_PLAN
        },

        {
            "05 - Unsupported folder location",
            "create a folder called NovaB2BenchBadLocation in Downloads",
            false,
            ExecutionStatus::NOT_EXECUTED,
            ExecutionFailureReason::LOW_CONFIDENCE
        },

        {
            "06 - Open Chrome",
            "open chrome",
            true,
            ExecutionStatus::SUCCESS,
            ExecutionFailureReason::NONE
        },

        {
            "07 - Open PowerPoint",
            "open powerpoint",
            true,
            ExecutionStatus::SUCCESS,
            ExecutionFailureReason::NONE
        },

        {
            "08 - Open Visual Studio Code",
            "open visual studio code",
            true,
            ExecutionStatus::SUCCESS,
            ExecutionFailureReason::NONE
        },

        {
            "09 - Missing application",
            "open",
            false,
            ExecutionStatus::NOT_EXECUTED,
            ExecutionFailureReason::INVALID_PLAN
        },

        {
            "10 - Unsupported action",
            "delete everything",
            false,
            ExecutionStatus::NOT_EXECUTED,
            ExecutionFailureReason::UNSUPPORTED_ACTION
        },

        {
            "11 - Unsupported close action",
            "close spotify",
            false,
            ExecutionStatus::NOT_EXECUTED,
            ExecutionFailureReason::UNSUPPORTED_ACTION
        },

        {
            "12 - Unknown action",
            "do something completely unsupported",
            false,
            ExecutionStatus::NOT_EXECUTED,
            ExecutionFailureReason::INVALID_PLAN
        },

        {
            "13 - Folder with invalid Windows character",
            "create a folder called NovaB2Bench<> on my desktop",
            false,
            ExecutionStatus::NOT_EXECUTED,
            ExecutionFailureReason::INVALID_PLAN
        },

        {
            "14 - Folder with reserved device name",
            "create a folder called CON on my desktop",
            false,
            ExecutionStatus::FAILED,
            ExecutionFailureReason::EXECUTION_ERROR
        },

        {
            "15 - Nested path attempt",
            "create a folder called NovaB2Bench\\Nested on my desktop",
            false,
            ExecutionStatus::FAILED,
            ExecutionFailureReason::EXECUTION_ERROR
        }
    };

    SingleActionPipeline pipeline;

    int passed = 0;
    int failed = 0;

    for (const auto& test : tests)
    {
        cout << "------------------------------------------------------------\n";
        cout << test.name << "\n";
        cout << "INPUT: " << test.input << "\n";
        cout << "EXPECTED: "
             << statusToString(test.expectedStatus)
             << " / "
             << reasonToString(test.expectedReason)
             << "\n";

        SingleActionResult result =
            pipeline.process(test.input);

        cout << "ACTUAL:   "
             << statusToString(result.executionResult.status)
             << " / "
             << reasonToString(result.executionResult.reason)
             << "\n";

        cout << "SUCCESS:  "
             << (result.success ? "true" : "false")
             << "\n";

        cout << "ACTION:   "
             << result.action.name
             << "\n";

        cout << "CONFIDENCE: "
             << result.executionResult.confidence
             << "\n";

        cout << "TARGET:   "
             << (result.executionResult.targetResolved ? "true" : "false")
             << "\n";

        cout << "MECHANISM: "
             << (result.executionResult.executionMechanismResolved ? "true" : "false")
             << "\n";

        cout << "MESSAGE:  "
             << result.executionResult.message
             << "\n";

        if (matches(result, test))
        {
            cout << "RESULT:   PASS\n";
            ++passed;
        }
        else
        {
            cout << "RESULT:   FAIL\n";
            ++failed;
        }
    }

    cout << "\n============================================================\n";
    cout << "BRANCH 2 BENCHMARK SUMMARY\n";
    cout << "============================================================\n";
    cout << "TOTAL:    " << tests.size() << "\n";
    cout << "PASSED:   " << passed << "\n";
    cout << "FAILED:   " << failed << "\n";

    if (failed == 0)
    {
        cout << "VERDICT:  ALL TESTS PASSED\n";
        return 0;
    }

    cout << "VERDICT:  FAILURES DETECTED\n";
    return 1;
}
