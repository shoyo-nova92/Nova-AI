#pragma once
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

struct PendingUserDecision {
    std::string questionId,stepId,reason,intent,searchEvidenceId,workflowState,preferredCandidateId;
    nlohmann::json candidates=nlohmann::json::array();
    nlohmann::json context=nlohmann::json::object();
    std::vector<std::string> allowedActions,targetFields;
    nlohmann::json json() const {
        return {{"question_id",questionId},{"step_id",stepId},{"reason",reason},{"intent",intent},
            {"context",context},{"workflow_state",workflowState},{"search_evidence_id",searchEvidenceId},{"preferred_candidate_id",preferredCandidateId},{"candidates",candidates},{"target_fields",targetFields},{"allowed_actions",allowedActions}};
    }
};
