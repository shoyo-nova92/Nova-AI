#pragma once

#include <functional>
#include <string>

#include "multistep_plan.hpp"
#include "llm_client.hpp"

using namespace std;

using PrimitiveMapperLLM = function<LLMResponse(
    const string& systemPrompt,
    const string& userPrompt)>;

class PrimitiveMapper {
public:
    PrimitiveMapper();
    explicit PrimitiveMapper(PrimitiveMapperLLM generator);

    // Maps a complete plan (legacy compatibility). Never executes actions.
    MultiStepPlan map(const MultiStepPlan& decomposedPlan);

    // Validates one planner-proposed operation without another LLM call.
    MultiStepPlan mapStep(const MultiStepAction& sourceStep, const std::string& operationJson);

private:
    PrimitiveMapperLLM generator_;
};
