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

    // Maps the entire decomposed plan in one LLM call. It never executes actions.
    MultiStepPlan map(const MultiStepPlan& decomposedPlan);

private:
    PrimitiveMapperLLM generator_;
};
