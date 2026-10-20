#include <windows.h>
#include <shlobj.h>

#include "branch3_runtime.hpp"

#include <nlohmann/json.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

using namespace std;
using nlohmann::json;
namespace fs = std::filesystem;

static fs::path getDesktop() {
    PWSTR raw = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_Desktop, 0, nullptr, &raw)) || !raw) return {};
    fs::path result(raw);
    CoTaskMemFree(raw);
    return result;
}

int main() {
    int failures = 0;
    int checks = 0;
    auto check = [&](const string& label, bool condition) {
        ++checks;
        cout << "[" << label << "] " << (condition ? "PASS" : "FAIL") << '\n';
        if (!condition) ++failures;
    };

    const fs::path desktop = getDesktop();
    if (desktop.empty()) {
        cerr << "Desktop path unavailable; cannot run integration test.\n";
        return 2;
    }

    const string folder = "NovaBranch3RuntimeTest_" +
        to_string(chrono::steady_clock::now().time_since_epoch().count());
    const fs::path workspace = desktop / folder;
    error_code ec;
    if (fs::exists(workspace)) {
        cerr << "Unique test workspace collision; refusing to touch existing path.\n";
        return 2;
    }

    int decomposerCalls = 0;
    int mapperCalls = 0;

    // Build fixture JSON structurally. This avoids hand-concatenation bugs
    // when the unique test folder name is embedded in a JSON string.
    const string decomposedJson = json{
        {"steps", json::array({
            json{{"id", "step_1"},
                 {"description", "Create folder " + folder},
                 {"depends_on", json::array()}},
            json{{"id", "step_2"},
                 {"description", "Create Main.java inside " + folder},
                 {"depends_on", json::array({"step_1"})}}
        })}
    }.dump();

    const string mappedJson = json{
        {"steps", json::array({
            json{{"id", "step_1"},
                 {"operations", json::array({
                     json{{"name", "filesystem.create"},
                          {"parameters", json{{"name", folder},
                                              {"location", "desktop"}}}}
                 })}},
            json{{"id", "step_2"},
                 {"operations", json::array({
                     json{{"name", "filesystem.create_file"},
                          {"parameters", json{{"name", "Main.java"},
                                              {"location", "desktop/" + folder},
                                              {"content", "class Main {}\n"}}}}
                 })}}
        })}
    }.dump();

    GoalDecomposerLLM decomposer = [&](const string&, const string&) {
        ++decomposerCalls;
        return LLMResponse{true, decomposedJson, ""};
    };
    PrimitiveMapperLLM mapper = [&](const string&, const string&) {
        ++mapperCalls;
        return LLMResponse{true, mappedJson, ""};
    };
    int plannerCalls = 0;
    NextActionLLM planner = [&](const string&, const string&) {
        ++plannerCalls;
        if (plannerCalls == 1) return LLMResponse{true, json{{"status","operation"},{"step_id","step_1"},{"operation",json{{"name","filesystem.create"},{"parameters",json{{"name",folder},{"location","desktop"}}}}},{"message","Create the workspace"}}.dump(), ""};
        if (plannerCalls == 2) return LLMResponse{true, json{{"status","operation"},{"step_id","step_2"},{"operation",json{{"name","filesystem.create_file"},{"parameters",json{{"name","Main.java"},{"location","desktop/"+folder},{"content","class Main {}\n"}}}}},{"message","Create the file"}}.dump(), ""};
        return LLMResponse{true, json{{"status","done"},{"message","Test artifacts are created."}}.dump(), ""};
    };

    Branch3Runtime runtime(decomposer, mapper, planner);
    const auto result = runtime.run("Create folder " + folder + " and create Main.java inside it.");

    if (!result.success) {
        cerr << "\nPRIMARY RUNTIME FAILURE: " << result.message << '\n'
             << "DECOMPOSER MESSAGE: " << result.decomposedPlan.message << '\n'
             << "MAPPER MESSAGE: " << result.mappedPlan.message << '\n'
             << "DECOMPOSER CALLS: " << decomposerCalls
             << ", MAPPER CALLS: " << mapperCalls << "\n\n";
    }

    check("01_RUNTIME_SUCCEEDED", result.success);
    check("02_PLANNING_SUCCEEDED", result.planningSuccess);
    check("03_EXECUTION_STARTED", result.executionStarted);
    check("04_EXACTLY_ONE_DECOMPOSER_CALL", decomposerCalls == 1);
    check("05_LEGACY_MAPPER_NOT_CALLED", mapperCalls == 0);
    check("06_THREE_PLANNER_DECISIONS", plannerCalls == 3);
    check("07_TWO_LOGICAL_STEPS", result.mappedPlan.steps.size() == 2);
    check("08_TWO_EXECUTED_PRIMITIVES", result.operationResults.size() == 2);
    check("09_FOLDER_EXISTS_ON_DISK", fs::is_directory(workspace));
    check("10_FILE_EXISTS_ON_DISK", fs::is_regular_file(workspace / "Main.java"));

    if (fs::is_regular_file(workspace / "Main.java")) {
        ifstream input(workspace / "Main.java", ios::binary);
        const string content((istreambuf_iterator<char>(input)), istreambuf_iterator<char>());
        check("11_FILE_CONTENT_VERIFIED", content == "class Main {}\n");
    } else {
        check("11_FILE_CONTENT_VERIFIED", false);
    }

    // An unsupported primitive must be rejected before execution.
    int badDecomposerCalls = 0;
    int badMapperCalls = 0;
    GoalDecomposerLLM badDecomposer = [&](const string&, const string&) {
        ++badDecomposerCalls;
        const string response = json{
            {"steps", json::array({
                json{{"id", "step_1"}, {"description", "Create a folder"}}
            })}
        }.dump();
        return LLMResponse{true, response, ""};
    };
    PrimitiveMapperLLM badMapper = [&](const string&, const string&) {
        ++badMapperCalls;

        // Construct this fixture incrementally to avoid ambiguous nested
        // initializer-list braces with nlohmann::json.
        json badOperation = json::object();
        badOperation["name"] = "filesystem.dangerous";
        badOperation["parameters"] = json::object();

        json badStep = json::object();
        badStep["id"] = "step_1";
        badStep["operations"] = json::array();
        badStep["operations"].push_back(badOperation);

        json badRoot = json::object();
        badRoot["steps"] = json::array();
        badRoot["steps"].push_back(badStep);

        return LLMResponse{true, badRoot.dump(), ""};
    };

    NextActionLLM badPlanner = [&](const string&, const string&) {
        return LLMResponse{true, json{{"status","operation"},{"step_id","step_1"},{"operation",json{{"name","filesystem.dangerous"},{"parameters",json::object()}}},{"message","test invalid operation"}}.dump(), ""};
    };
    Branch3Runtime guardedRuntime(badDecomposer, badMapper, badPlanner);
    const auto guarded = guardedRuntime.run("Create a folder");
    check("12_UNSUPPORTED_PRIMITIVE_REJECTED", !guarded.success && guarded.executionStarted && guarded.operationResults.empty());
    check("13_BAD_PLAN_DID_NOT_CALL_LEGACY_MAPPER", badDecomposerCalls == 1 && badMapperCalls == 0);

    // The workspace path was confirmed absent before this test, so any contents
    // at this exact unique path belong to this test and can be safely removed.
    ec.clear();
    if (fs::exists(workspace)) fs::remove_all(workspace, ec);
    check("14_TEST_WORKSPACE_CLEANED", !fs::exists(workspace) && !ec);

    cout << "\nBRANCH 3 RUNTIME INTEGRATION TEST\nPassed: " << (checks - failures)
         << "\nFailed: " << failures << '\n'
         << (failures == 0 ? "RESULT: PASS\n" : "RESULT: FAIL\n");
    return failures == 0 ? 0 : 1;
}
