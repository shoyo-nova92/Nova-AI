#pragma once
#include "multistep_plan.hpp"
#include "llm_client.hpp"
#include <functional>
#include <string>

using NextActionLLM = std::function<LLMResponse(const std::string&, const std::string&)>;
struct NextActionDecision {
    LLMFailureCategory failureCategory = LLMFailureCategory::NONE;
    std::string status; // operation, done, ask_user, blocked
    std::string stepId;
    std::string operationJson;
    std::string message;
    std::string evidence;
    std::string answer;
    std::string verificationJson;
};
class NextActionPlanner {
public:
    NextActionPlanner();
    explicit NextActionPlanner(NextActionLLM generator);
    NextActionDecision choose(const std::string& goal, const MultiStepPlan& guide,
                              const std::string& state, const std::string& catalog,
                              const std::string& selectedStepId = {});
private:
    NextActionLLM generator_;
};
