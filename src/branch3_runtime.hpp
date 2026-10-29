#pragma once

#include "goal_decomposer.hpp"
#include "primitive_mapper.hpp"
#include "primitive_execution_engine.hpp"
#include "next_action_planner.hpp"
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include "runtime_outcome.hpp"
#include "pending_user_decision.hpp"
#include "strategy_recovery.hpp"
#include <optional>

struct Branch3OperationResult {
    std::string stepId;
    std::size_t operationIndex = 0;
    ExecutionResult execution;
};

struct Branch3RuntimeResult : TaskResult {
    bool success = false; // Compatibility view, set only from outcome == COMPLETED.
    bool planningSuccess = false;
    bool executionStarted = false;
    MultiStepPlan decomposedPlan{};
    MultiStepPlan mappedPlan{};
    std::vector<Branch3OperationResult> operationResults;
    std::string stateLog;
};
struct RuntimeControls {
    bool answer=false,guidance=false,reobserve=false,retry=false;
};

class Branch3Runtime {
public:
    Branch3Runtime();
    Branch3Runtime(GoalDecomposerLLM decomposerGenerator,
                   PrimitiveMapperLLM mapperGenerator,
                   NextActionLLM plannerGenerator = {},
                   std::function<ExecutionResult(const PrimitiveOperation&)> executor = {},
                   NextActionLLM answerInterpreter = {});
    ~Branch3Runtime();
    Branch3RuntimeResult start(const std::string& goal);
    Branch3RuntimeResult continueSession();
    Branch3RuntimeResult retryBlockedDependency();
    bool hasActiveSession() const;
    RuntimeControls controls() const;
    std::optional<PendingUserDecision> pendingUserDecision() const;
    Branch3RuntimeResult requestGuidance();
    Branch3RuntimeResult answerUser(const std::string& questionId,const std::string& answer);
    void abandonSession();
    void setRecoverySink(std::function<void(const nlohmann::json&)> sink) { recoverySink_=std::move(sink); }
    void setRecoveryPolicy(StrategyRecovery::Policy policy) {
        policy.autonomousAttempts=std::clamp(policy.autonomousAttempts,2U,8U);policy.guidedAttempts=std::clamp(policy.guidedAttempts,1U,12U);
        policy.totalAttempts=std::clamp(policy.totalAttempts,4U,32U);policy.questions=std::clamp(policy.questions,1U,5U);recoveryPolicy_=policy;
    }
    void setEventSink(std::function<void(const std::string&)> sink) { eventSink_=std::move(sink); }

    // Decomposes once, then repeatedly observes, plans one operation, validates, and executes.
    Branch3RuntimeResult run(const std::string& goal);

private:
    struct Session;
    std::unique_ptr<Session> session_;
    Branch3RuntimeResult advance();
    Branch3RuntimeResult resumeSession(bool refreshBackend);
    Branch3RuntimeResult advanceImpl();
    std::function<ExecutionResult(const PrimitiveOperation&)> executor_;
    GoalDecomposer decomposer_;
    PrimitiveMapper mapper_;
    NextActionPlanner planner_;
    NextActionLLM answerInterpreter_;
    bool observeEnvironment_ = false;
    std::function<void(const std::string&)> eventSink_;
    void recordEvent(const std::string& event);
    void recoveryTransition(const std::string& stepId,const std::string& status,const std::string& outcome);
    Branch3RuntimeResult askStrategyGuidance(const std::string& stepId,const std::string& blocker);
    StrategyRecovery::Policy recoveryPolicy_;
    std::function<void(const nlohmann::json&)> recoverySink_;
};
