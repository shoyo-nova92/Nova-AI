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

int main() {
    int failures = 0;
    int checks = 0;
    auto check = [&](const string& label, bool condition) {
        ++checks;
        cout << "[" << label << "] " << (condition ? "PASS" : "FAIL") << '\n';
        if (!condition) ++failures;
    };

    const string folder = "NovaBranch3RuntimeTest_" +
        to_string(chrono::steady_clock::now().time_since_epoch().count());
    const fs::path base = fs::temp_directory_path();
    const fs::path workspace = base / folder;
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
                 {"postcondition",{{"kind","directory_exists"},{"name",folder},{"location",base.string()}}},
                 {"depends_on", json::array()}},
            json{{"id", "step_2"},
                 {"description", "Create Main.java inside " + folder},
                 {"postcondition",{{"kind","file_content"},{"name","Main.java"},{"location",workspace.string()}}},
                 {"content_requirement","required"},
                 {"depends_on", json::array({"step_1"})}}
        })}
    }.dump();

    const string mappedJson = json{
        {"steps", json::array({
            json{{"id", "step_1"},
                 {"operations", json::array({
                     json{{"name", "filesystem.create"},
                          {"parameters", json{{"name", folder},
                                              {"location", base.string()}}}}
                 })}},
            json{{"id", "step_2"},
                 {"operations", json::array({
                     json{{"name", "filesystem.create_file"},
                          {"parameters", json{{"name", "Main.java"},
                                              {"location", workspace.string()},
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
        if (plannerCalls == 1) return LLMResponse{true, json{{"status","operation"},{"step_id","step_1"},{"operation",json{{"name","filesystem.create"},{"parameters",json{{"name",folder},{"location",base.string()}}}}},{"message","Create the workspace"}}.dump(), ""};
        if (plannerCalls == 2) return LLMResponse{true, json{{"status","operation"},{"step_id","step_2"},{"operation",json{{"name","filesystem.create_file"},{"parameters",json{{"name","Main.java"},{"location",workspace.string()},{"content","class Main {}\n"}}}}},{"message","Create the file"}}.dump(), ""};
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
        cerr << result.stateLog;
    }

    check("01_RUNTIME_SUCCEEDED", result.success);
    check("02_PLANNING_SUCCEEDED", result.planningSuccess);
    check("03_EXECUTION_STARTED", result.executionStarted);
    check("04_EXACTLY_ONE_DECOMPOSER_CALL", decomposerCalls == 1);
    check("05_LEGACY_MAPPER_NOT_CALLED", mapperCalls == 0);
    check("06_VERIFIED_ARTIFACTS_NEED_TWO_PLANNER_DECISIONS", plannerCalls == 2);
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
    check("12_UNSUPPORTED_PRIMITIVE_REJECTED", !guarded.success && !guarded.executionStarted && guarded.operationResults.empty() && guarded.cause==RuntimeCause::UNSUPPORTED_CAPABILITY);
    check("13_BAD_PLAN_DID_NOT_CALL_LEGACY_MAPPER", badDecomposerCalls == 1 && badMapperCalls == 0);

    // Content intent comes from semantic decomposition, independent of names/extensions.
    auto contentScenario = [&](const string& requirement, const string& name, int mode) {
        int calls = 0;
        const string body = "int transform(int value) { return value * 2; }\n";
        GoalDecomposerLLM interpret = [&](const string&, const string&) {
            return LLMResponse{true, json{{"steps", json::array({json{{"id","step_1"},
                {"description","Create the requested artifact with its requested contents"},
                {"content_requirement",requirement}}})}}.dump(), ""};
        };
        NextActionLLM plan = [&](const string&, const string& context) {
            ++calls;
            if (mode == 4 && calls == 2)
                return LLMResponse{true, json{{"status","done"}}.dump(), ""};
            if ((mode == 0 || mode == 1) && calls > 1 || mode == 2 && calls > 2)
                return LLMResponse{true, json{{"status","done"},{"message","Artifact verified in execution history"}}.dump(), ""};
            json params = {{"name",name},{"location",workspace.string()}};
            if (mode == 1 || mode == 2 && calls > 1) params["content"] = body;
            if (mode == 0) params["content"] = "";
            if (mode == 5) params["content"] = " \t\r\n";
            if (mode == 2 && calls == 2)
                check("REPAIR_RECEIVES_VALIDATION_FEEDBACK", context.find("requires populated content") != string::npos);
            return LLMResponse{true, json{{"status","operation"},{"step_id","step_1"},
                {"operation",json{{"name","filesystem.create_file"},{"parameters",params}}}}.dump(), ""};
        };
        Branch3Runtime subject(interpret, mapper, plan);
        const auto outcome = subject.run("Create requested artifact");
        if (mode <= 2) {
            check("CONTENT_SCENARIO_SUCCEEDS_" + name, outcome.success);
            ifstream file(workspace / name, ios::binary);
            const string actual((istreambuf_iterator<char>(file)), istreambuf_iterator<char>());
            check("CONTENT_READBACK_" + name, file.is_open() && actual == (mode == 0 ? "" : body));
        } else {
            check("INVALID_CONTENT_FAILS_" + name, !outcome.success && outcome.operationResults.empty() &&
                !fs::exists(workspace / name) && calls == 3);
        }
    };
    contentScenario("empty_allowed", "empty.data", 0);
    contentScenario("required", "generated.cpp", 1);
    contentScenario("required", "repaired.data", 2);
    contentScenario("required", "omitted.data", 3);
    contentScenario("required", "false_done.data", 4);
    contentScenario("required", "whitespace.data", 5);

    PrimitiveExecutionEngine writeEngine;
    PrimitiveOperation write{};
    write.name = "text.write";
    write.type = PrimitiveOperationType::TEXT_WRITE;
    write.resolved = true;
    write.parameters.target = (workspace / "empty.data").string();
    write.parameters.content = "Updated existing artifact";
    const auto written = writeEngine.execute(write);
    ifstream existing(workspace / "empty.data", ios::binary);
    const string updated((istreambuf_iterator<char>(existing)), istreambuf_iterator<char>());
    if (written.status != ExecutionStatus::SUCCESS) cerr << written.message << "\n";
    existing.close();
    check("EXISTING_FILE_WRITE", written.status == ExecutionStatus::SUCCESS && updated == write.parameters.content);

    MultiStepAction probe{};
    probe.id = "step_1";
    probe.description = "Create file";
    PrimitiveMapper direct;
    const json probeParams = {{"name","probe.data"},{"location",workspace.string()}};
    auto operationJson = json{{"name","filesystem.create_file"},{"parameters",probeParams}};
    check("MAPPER_DOES_NOT_INSERT_MISSING_CONTENT", direct.mapStep(probe, operationJson.dump()).valid);
    operationJson["parameters"]["content"] = "";
    check("PLANNER_SUPPLIED_EMPTY_VALUE_CAUSES_SCHEMA_ERROR", !direct.mapStep(probe, operationJson.dump()).valid);

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
