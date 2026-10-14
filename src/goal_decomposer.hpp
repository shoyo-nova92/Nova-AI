#pragma once

#include "multistep_plan.hpp"
#include "llm_client.hpp"

#include <functional>
#include <string>

using namespace std;

using GoalDecomposerLLM = function<LLMResponse(
    const string& systemPrompt,
    const string& userPrompt
)>;

class GoalDecomposer
{
public:
    GoalDecomposer();

    explicit GoalDecomposer(GoalDecomposerLLM generator);

    MultiStepPlan decompose(const string& goal);

private:
    GoalDecomposerLLM generator_;
};
