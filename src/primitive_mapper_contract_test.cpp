#include "primitive_mapper.hpp"

#include <iostream>
#include <string>
#include <vector>

using namespace std;

namespace {
int passed = 0;
int failed = 0;

void check(const string& name, bool condition) {
    cout << "[" << name << "] " << (condition ? "PASS" : "FAIL") << '\n';
    condition ? ++passed : ++failed;
}

LLMResponse responseWith(const string& content) {
    LLMResponse response;
    response.success = true;
    response.content = content;
    response.message.clear();
    return response;
}

MultiStepPlan makePlan() {
    MultiStepPlan plan{};
    plan.valid = true;
    plan.goal = "Create a project folder, then create a source file in it.";
    plan.message.clear();

    MultiStepAction first{};
    first.id = "step_1";
    first.description = "Create the project folder named NovaDemo on the desktop";
    first.resolved = false;

    MultiStepAction second{};
    second.id = "step_2";
    second.description = "Create Main.java inside the NovaDemo folder";
    second.dependencies = {"step_1"};
    second.resolved = false;

    plan.steps = {first, second};
    return plan;
}

string validMapping() {
    return R"JSON({
      "steps": [
        {"id":"step_1","operations":[{"name":"filesystem.create","parameters":{"name":"NovaDemo","location":"desktop"}}]},
        {"id":"step_2","operations":[{"name":"filesystem.create_file","parameters":{"name":"Main.java","location":"desktop/NovaDemo","content":"public class Main {}"}}]}
      ]
    })JSON";
}
}

int main() {
    int llmCalls = 0;
    string capturedSystem;
    string capturedUser;
    PrimitiveMapper mapper([&](const string& systemPrompt, const string& userPrompt) {
        ++llmCalls;
        capturedSystem = systemPrompt;
        capturedUser = userPrompt;
        return responseWith(validMapping());
    });

    const MultiStepPlan source = makePlan();
    const MultiStepPlan mapped = mapper.map(source);
    check("LLM_CALLED_EXACTLY_ONCE", llmCalls == 1);
    check("WHOLE_PLAN_INCLUDED", capturedUser.find("step_1") != string::npos &&
          capturedUser.find("step_2") != string::npos && capturedUser.find(source.goal) != string::npos);
    check("LIVE_CATALOG_INCLUDED", capturedUser.find("LIVE_PRIMITIVE_REGISTRY") != string::npos &&
          capturedUser.find("filesystem.create_file") != string::npos);
    check("PROMPT_FORBIDS_INVENTED_PRIMITIVES", capturedSystem.find("Use only primitive names") != string::npos);
    check("VALID_MAPPING_ACCEPTED", mapped.valid && mapped.steps.size() == 2);
    check("OPERATIONS_RESOLVED", mapped.valid && mapped.steps[0].operations.size() == 1 &&
          mapped.steps[0].operations[0].resolved && mapped.steps[1].operations[0].resolved);
    check("DEPENDENCIES_PRESERVED", mapped.valid && mapped.steps[1].dependencies.size() == 1 &&
          mapped.steps[1].dependencies[0] == "step_1");
    check("STEP_IDS_PRESERVED", mapped.valid && mapped.steps[0].id == "step_1" && mapped.steps[1].id == "step_2");

    PrimitiveMapper invented([](const string&, const string&) {
        return responseWith(R"JSON({"steps":[
          {"id":"step_1","operations":[{"name":"filesystem.magic","parameters":{}}]},
          {"id":"step_2","operations":[{"name":"filesystem.create_file","parameters":{"name":"Main.java","location":"desktop/NovaDemo","content":"x"}}]}
        ]})JSON");
    });
    const auto inventedResult = invented.map(source);
    check("INVENTED_PRIMITIVE_REJECTED", !inventedResult.valid && inventedResult.steps.empty());

    PrimitiveMapper unknownParameter([](const string&, const string&) {
        return responseWith(R"JSON({"steps":[
          {"id":"step_1","operations":[{"name":"filesystem.create","parameters":{"name":"NovaDemo","location":"desktop","surprise":"x"}}]},
          {"id":"step_2","operations":[{"name":"filesystem.create_file","parameters":{"name":"Main.java","location":"desktop/NovaDemo","content":"x"}}]}
        ]})JSON");
    });
    const auto unknownParameterResult = unknownParameter.map(source);
    check("UNKNOWN_PARAMETER_REJECTED", !unknownParameterResult.valid && unknownParameterResult.steps.empty());

    PrimitiveMapper wrongOrder([](const string&, const string&) {
        return responseWith(R"JSON({"steps":[
          {"id":"step_2","operations":[{"name":"filesystem.create_file","parameters":{"name":"Main.java","location":"desktop/NovaDemo","content":"x"}}]},
          {"id":"step_1","operations":[{"name":"filesystem.create","parameters":{"name":"NovaDemo","location":"desktop"}}]}
        ]})JSON");
    });
    const auto wrongOrderResult = wrongOrder.map(source);
    check("STEP_REORDERING_REJECTED", !wrongOrderResult.valid && wrongOrderResult.steps.empty());

    PrimitiveMapper malformed([](const string&, const string&) {
        return responseWith("{not-json");
    });
    const auto malformedResult = malformed.map(source);
    check("MALFORMED_JSON_REJECTED", !malformedResult.valid && malformedResult.steps.empty());

    PrimitiveMapper failedLLM([](const string&, const string&) {
        LLMResponse response;
        response.success = false;
        response.message = "simulated provider failure";
        return response;
    });
    const auto failedResult = failedLLM.map(source);
    check("LLM_FAILURE_REJECTED", !failedResult.valid && failedResult.steps.empty());
    check("LLM_FAILURE_MESSAGE_PRESERVED", failedResult.message == "simulated provider failure");

    MultiStepPlan invalid{};
    invalid.valid = false;
    invalid.goal = "invalid";
    int invalidPlanCalls = 0;
    PrimitiveMapper shouldNotCall([&](const string&, const string&) {
        ++invalidPlanCalls;
        return responseWith(validMapping());
    });
    const auto invalidResult = shouldNotCall.map(invalid);
    check("INVALID_PLAN_REJECTED_BEFORE_LLM", !invalidResult.valid && invalidPlanCalls == 0);

    cout << "\nPRIMITIVE MAPPER CONTRACT TEST\nPassed: " << passed
         << "\nFailed: " << failed << '\n';
    return failed == 0 ? 0 : 1;
}
