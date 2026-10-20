#pragma once
#include "multistep_plan.hpp"
#include "llm_client.hpp"
#include <functional>
#include <string>

using NextActionLLM = std::function<LLMResponse(const std::string&, const std::string&)>;
struct NextActionDecision {
    std::string status; // operation, done, ask_user, blocked
    std::string stepId;
    std::string operationJson;
    std::string message;
};
class NextActionPlanner {
public:
    NextActionPlanner();
    explicit NextActionPlanner(NextActionLLM generator);
    NextActionDecision choose(const std::string& goal, const MultiStepPlan& guide,
                              const std::string& state, const std::string& catalog);
private:
    NextActionLLM generator_;
};
