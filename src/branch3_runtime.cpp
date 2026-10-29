#include "environment_snapshot.hpp"
#include "branch3_runtime.hpp"
#include "workflow_kernel.hpp"
#include "artifact_binding.hpp"
#include "semantic_evidence.hpp"
#include "predicate_registry.hpp"
#include "filesystem_path_policy.hpp"
#include "filesystem_location_resolver.hpp"
#include "primitive_registry.hpp"
#include "state_manager.hpp"
#include "browser_state.hpp"
#include "application_state.hpp"
#include "application_correlation.hpp"
#include "utf8_text.hpp"
#include "filesystem_ranking.hpp"
#include <algorithm>
#include <filesystem>
#include <set>
#include <map>
#include <sstream>
#include <cwctype>
#include <chrono>
#include <atomic>
using nlohmann::json;
namespace wf=workflow;
namespace {
std::string liveCatalog() {
    json catalog=json::array();
    for(const auto& [name,def]:PrimitiveRegistry::getCatalog()) {
        json item={{"name",name},{"description",def.description},{"capability",def.capability},{"parameters",json::array()}};
        if(def.parameters) for(const auto& f:def.parameters->fields)
            item["parameters"].push_back({{"name",f.name},{"requirement",f.requirement==ParameterRequirement::REQUIRED?"required":f.requirement==ParameterRequirement::OPTIONAL?"optional":"not_applicable"},{"description",f.description},{"allowed_values",f.allowedValues}});
        Utf8::validate(item,"registry."+name);catalog.push_back(std::move(item));
    }
    return Utf8::dump(catalog,"registry",2);
}
std::filesystem::path artifactParent(const std::string& location) {
    const FilesystemPathPolicy policy;const auto path=policy.resolve(location);
    if(path.ok){const auto canonical=policy.authorize(path.path,FilesystemAccess::DISCOVER);return canonical.ok?canonical.path:std::filesystem::path{};}
    const auto found=FilesystemLocationResolver{}.resolve(location);
    if(found.status!=FilesystemLocationResolution::RESOLVED)return {};
    const auto canonical=policy.authorize(found.path,FilesystemAccess::DISCOVER);return canonical.ok?canonical.path:std::filesystem::path{};
}
std::vector<wf::Step> workflowSteps(const MultiStepPlan& plan) {
    std::vector<wf::Step> steps;
    for(const auto& source:plan.steps) {
        wf::Step step;step.id=source.id;step.intent=source.description;step.dependencies=source.dependencies;
        if(!source.conditionJson.empty()) {
            const auto c=json::parse(source.conditionJson);
            if(!c.is_null())step.condition=wf::Condition{c.at("step_id").get<std::string>(),c.at("field").get<std::string>(),c.at("equals")};
        }
        if(!source.semanticPostconditionJson.empty()) {
            auto proof=json::parse(source.semanticPostconditionJson);std::string error;
            // Known-folder clues are resolved by the host, not reconstructed by a planner.
            if(proof.value("source",std::string{})=="filesystem") {
                auto bindRoot=[&](json& object) {
                    if(object.contains("search_root")) {
                        const auto root=artifactParent(object["search_root"].get<std::string>());
                        if(root.empty())throw std::invalid_argument("Cannot resolve search root for "+source.id);
                        object["search_root"]=Utf8::path(root);
                    }
                };
                bindRoot(proof);if(proof.contains("match"))bindRoot(proof["match"]);
            }
            step.postconditions=SemanticEvidence::compile(proof,error);
            if(step.postconditions.empty())throw std::invalid_argument("Invalid postcondition for "+source.id+": "+error);
        }else if(!source.postconditionKind.empty()) {
            wf::Postcondition p;p.kind=wf::PredicateKind::ARTIFACT;p.value=source.postconditionKind;
            if(source.artifactParentReference) {p.pathFromStep=source.artifactParentReference->sourceStepId;p.artifactLeaf=source.artifactName;}
            else {
                const auto parent=artifactParent(source.artifactLocation);
                if(parent.empty())throw std::invalid_argument("Cannot resolve artifact parent for "+source.id+": "+source.artifactLocation);
                p.path=Utf8::path((parent/Utf8::toPath(source.artifactName)).lexically_normal());
            }
            step.postconditions.push_back(p);
        } else if(source.contentRequirement=="required" || source.contentRequirement=="empty_allowed") {
            wf::Postcondition p;p.kind=wf::PredicateKind::ARTIFACT;p.value="file_content";step.postconditions.push_back(p);
        }
        steps.push_back(std::move(step));
    }
    return steps;
}
const MultiStepAction& originalStep(const MultiStepPlan& plan,const std::string& id) {
    for(const auto& step:plan.steps)if(step.id==id)return step;
    throw std::logic_error("Kernel selected unknown source step: "+id);
}
RuntimeCause backendCause(const std::string& code) {
    if(code=="invalid_utf8")return RuntimeCause::INVALID_UTF8;
    if(code=="stale_element")return RuntimeCause::STALE_ELEMENT;
    if(code=="browser_timeout")return RuntimeCause::BROWSER_TIMEOUT;
    if(code=="auth_required")return RuntimeCause::AUTH_REQUIRED;
    if(code=="target_not_found" || code=="not_found")return RuntimeCause::TARGET_NOT_FOUND;
    if(code=="already_exists")return RuntimeCause::ALREADY_EXISTS;
    if(code=="unsupported_action")return RuntimeCause::UNSUPPORTED_CAPABILITY;
    if(code=="access_denied" || code=="protected_location")return RuntimeCause::ACCESS_DENIED;
    return RuntimeCause::UNKNOWN;
}
bool transient(LLMFailureCategory c) {
    return c==LLMFailureCategory::TIMEOUT || c==LLMFailureCategory::RATE_LIMIT || c==LLMFailureCategory::PROVIDER_ERROR || c==LLMFailureCategory::CONNECTION_ERROR;
}
bool readOnly(const PrimitiveOperation& operation) {
    return operation.name=="application.observe" || operation.name=="environment.observe" ||
        operation.name=="filesystem.inspect" || operation.name=="filesystem.search" || operation.name=="filesystem.list_directory" ||
        operation.name=="browser.observe" || operation.name=="browser.tabs" || operation.name=="browser.wait_for";
}
std::string operationKey(const PrimitiveOperation& operation,const json& proposed) {
    // Launch identity is independent of JSON field order and Windows path casing.
    if(operation.name=="application.launch" || operation.name=="application.observe") {
        auto app=Utf8::toPath(operation.parameters.application).wstring();
        auto target=Utf8::toPath(operation.parameters.target).lexically_normal().wstring();
        std::transform(app.begin(),app.end(),app.begin(),::towlower);
        std::transform(target.begin(),target.end(),target.begin(),::towlower);
        return operation.name+json::array({Utf8::path(std::filesystem::path(app)),Utf8::path(std::filesystem::path(target)),operation.parameters.processId,operation.parameters.windowHandle}).dump();
    }
    return operation.name+proposed.value("parameters",json::object()).dump();
}
wf::Evidence executionEvidence(const std::string& stepId,const PrimitiveOperation& operation,const ExecutionResult& result) {
    wf::Evidence evidence;evidence.stepId=stepId;evidence.operation=operation.name;
    auto facts=json::parse(result.factsJson,nullptr,false);evidence.facts=facts.is_object()?facts:json::object();
    evidence.facts["operation_succeeded"]=result.status==ExecutionStatus::SUCCESS;
    evidence.facts["postcondition_verified"]=result.postconditionVerified;
    if(!result.resolvedPath.empty())evidence.facts["path"]=result.resolvedPath;
    if(!result.discoveredPaths.empty())evidence.facts["discovered_paths"]=result.discoveredPaths;
    if(result.postconditionVerified) {evidence.category=wf::EvidenceCategory::ARTIFACT;evidence.facts["verified_kind"]=result.verifiedKind;}
    else if(result.status!=ExecutionStatus::SUCCESS)evidence.category=wf::EvidenceCategory::FAILURE;
    else if(operation.name=="application.observe") {evidence.category=wf::EvidenceCategory::APPLICATION_OBSERVATION;}
    else if(operation.name.starts_with("application.")) {evidence.category=wf::EvidenceCategory::APPLICATION_DISPATCH;evidence.facts["dispatch_accepted"]=true;}
    // The adapter retains structured backend facts; it never parses execution prose.
    if(operation.name=="filesystem.copy" && result.postconditionVerified) {
        evidence.facts["source"]=operation.parameters.source;evidence.facts["destination"]=result.resolvedPath;
        evidence.facts["source_bytes_equal"]=result.verifiedKind=="file_content";
    }
    if((operation.name=="filesystem.rename" || operation.name=="filesystem.move") && result.postconditionVerified) {
        evidence.facts["source"]=operation.name=="filesystem.rename"?operation.parameters.target:operation.parameters.source;
        evidence.facts["destination"]=result.resolvedPath;evidence.facts["source_absent"]=true;
    }
    return evidence;
}
json discoveryCandidates(const wf::Evidence& evidence,const wf::Step* producer) {
    json candidates=json::array();
    for(const auto& row:evidence.facts.value("results",json::array())) {
        bool matches=true;
        if(producer)for(const auto& post:producer->postconditions)if(post.kind==wf::PredicateKind::SEMANTIC && post.value.value("source",std::string{})=="filesystem" && PredicateRegistry::kindOf(post.value).starts_with("search_")) {
            auto one=evidence;one.facts["results"]=json::array({row});std::string error;
            if(!SemanticEvidence::matches(one,post.value,error))matches=false;
        }
        if(matches)candidates.push_back(row);
    }
    return candidates;
}
std::vector<wf::Postcondition> interpretation(const NextActionDecision& decision,std::string& error) {
    const auto proof=json::parse(decision.verificationJson,nullptr,false);
    if(!proof.is_object())return {};
    wf::Postcondition p;
    const auto source=proof.value("source",std::string{});
    if(source=="execution") {
        p.operation=proof.value("operation",std::string{});p.path=proof.value("path",std::string{});
        if(p.operation.empty())return {};
        if(!PrimitiveRegistry::isSupported(p.operation)){error="Unsupported verification predicate: execution."+p.operation;return {};}
        if(p.operation=="filesystem.search") {error="Unsupported verification predicate: execution.filesystem.search; use structured filesystem search predicates";return {};}
        // Dispatch alone cannot prove browser semantic effects.
        if(p.operation.starts_with("browser.") && p.operation!="browser.download")return {};
        p.field=proof.value("field",std::string("operation_succeeded"));p.value=proof.value("equals",json(true));
    } else if(source=="browser" || source=="application" || source=="filesystem") {
        return SemanticEvidence::compile(proof,error);
    } else return {};
    if(p.operation=="browser.download") {
        wf::Postcondition completed;completed.operation=p.operation;completed.path=p.path;completed.field="postcondition_verified";completed.value=true;
        return {p,completed};
    }
    return {p};
}
std::string ledger(const wf::Kernel& kernel) {
    json steps=json::array(),facts=json::array();
    for(const auto& step:kernel.graph.steps()) {
        json item={{"id",step.id},{"status",wf::statusName(step.status)},{"dependencies",step.dependencies},{"evidence_ids",step.evidenceIds}};
        item["postconditions"]=json::array();for(const auto& predicate:step.postconditions) {
            if(predicate.kind==wf::PredicateKind::SEMANTIC)item["postconditions"].push_back(predicate.value);
            else if(predicate.kind==wf::PredicateKind::ARTIFACT)item["postconditions"].push_back({{"source","artifact"},{"kind",predicate.value},{"path",predicate.path},{"parent_from_step",predicate.pathFromStep},{"name",predicate.artifactLeaf}});
            else if(predicate.kind==wf::PredicateKind::INTERPRETED)item["postconditions"].push_back({{"mode","observed_interpretation_required"}});
        }
        if(step.recovery.active)item["recovery"]={{"cause",causeName(step.recovery.active->category)},{"code",step.recovery.active->code},{"message",step.recovery.active->message}};
        steps.push_back(item);
    }
    const auto& all=kernel.facts.all();const size_t begin=all.size()>24?all.size()-24:0;
    for(size_t i=begin;i<all.size();++i)if(all[i].current) {
        json item={{"id",all[i].id},{"step_id",all[i].stepId},{"operation",all[i].operation}};
        if(all[i].category==wf::EvidenceCategory::BROWSER_OBSERVATION) {
            item["facts_location"]="CURRENT_BROWSER_PROJECTION; complete stable facts retained in FactStore";
            item["available_fields"]=json::array();for(const auto& [key,value]:all[i].facts.items())item["available_fields"].push_back(key);
        }else item["facts"]=all[i].facts;
        facts.push_back(item);
    }
    return "\nWORKFLOW_STATE:\n"+Utf8::dump(steps,"workflow.state")+"\nSTRUCTURED_EVIDENCE:\n"+Utf8::dump(facts,"workflow.facts");
}
}
struct Branch3Runtime::Session {
    explicit Session(std::string goal):goal(std::move(goal)),state(this->goal) {
        static std::atomic<uint64_t> sequence{0};
        identity=std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+":"+std::to_string(++sequence);
    }
    std::string goal,environment;
    std::string identity;
    StateManager state;
    Branch3RuntimeResult result;
    std::unique_ptr<wf::Kernel> kernel;
    std::set<std::string> announced,staleReferences;
    std::map<std::string,unsigned> invalidDecisions;
    std::map<std::string,std::set<std::string>> successfulMutations;
    std::map<std::string,std::string> applicationFingerprints;
    std::map<std::string,PrimitiveOperation> applicationScopes;
    std::map<std::string,std::string> lastApplicationFingerprint;
    std::optional<PendingUserDecision> pending;
    std::optional<NextActionDecision> heldDecision,resumeDecision;
    std::map<std::string,NextActionDecision> searchDecisions;
    json searchCandidates=json::array();
    std::string resolvedTarget;
    std::set<std::string> confirmedTargets;
    unsigned questionSequence=0;
    std::map<std::string,std::string> rejectedProofs;
    std::set<std::string> proofSchemaRepairs;
    std::map<std::string,StrategyRecovery::StepContext> strategies;
    unsigned episodeSequence=0;
    std::optional<std::pair<std::string,std::string>> retryAnswer;
    bool refreshBeforePlanning=false;
    bool browserActive=false;
    std::string lastBrowserAction,beforeAction,lastFingerprint;
    unsigned noChange=0;
};
Branch3Runtime::Branch3Runtime():executor_([](const PrimitiveOperation& op){return PrimitiveExecutionEngine{}.execute(op);}),observeEnvironment_(true) {
    answerInterpreter_=[](const std::string& system,const std::string& user){return LLMClient{}.generate(system,user,"planner");};
    recoverySink_=[](const json& record){if(!StrategyRecovery::append("logs/recovery-episodes.jsonl",record))throw std::runtime_error("Recovery log unavailable");};
}
Branch3Runtime::~Branch3Runtime(){abandonSession();}
Branch3Runtime::Branch3Runtime(GoalDecomposerLLM decomposer,PrimitiveMapperLLM mapper,NextActionLLM planner,std::function<ExecutionResult(const PrimitiveOperation&)> executor,NextActionLLM answerInterpreter)
    :executor_(executor?std::move(executor):[](const PrimitiveOperation& op){return PrimitiveExecutionEngine{}.execute(op);}),decomposer_(std::move(decomposer)),mapper_(std::move(mapper)),planner_(planner?NextActionPlanner(std::move(planner)):NextActionPlanner()),answerInterpreter_(std::move(answerInterpreter)) {}
void Branch3Runtime::recordEvent(const std::string& event) {session_->result.stateLog+=event;if(eventSink_)eventSink_(event);}
bool Branch3Runtime::hasActiveSession()const{return session_!=nullptr;}
RuntimeControls Branch3Runtime::controls() const {
    RuntimeControls available;if(!session_)return available;const auto& s=*session_;
    const bool dependency=s.result.outcome==RuntimeOutcome::EXTERNAL_UNAVAILABLE || s.result.outcome==RuntimeOutcome::RETRYABLE;
    if(!s.kernel) {available.retry=dependency && s.result.decomposedPlan.failureCategory!=LLMFailureCategory::NONE;return available;}
    if(!s.kernel->graph.valid() || s.kernel->complete())return available;
    bool ready=false,applicationScope=false;
    for(const auto& step:s.kernel->graph.steps())if((step.status==wf::StepStatus::READY || step.status==wf::StepStatus::RETRYABLE) && s.kernel->graph.dependenciesSatisfied(step) &&
        (!step.condition || wf::WorkflowScheduler::evaluate(*step.condition,s.kernel->graph,s.kernel->facts)==wf::Truth::TRUE_VALUE)) {
        ready=true;applicationScope |= s.applicationScopes.contains(step.id);
        if(s.pending && s.pending->stepId==step.id)available.answer=true;
    }
    available.guidance=ready && !s.pending;
    available.reobserve=ready && !s.pending && (s.browserActive || applicationScope);
    available.retry=dependency && (s.pending?available.answer && s.retryAnswer.has_value():ready);
    return available;
}
void Branch3Runtime::abandonSession(){
    if(session_)for(const auto& [id,context]:session_->strategies)
        if(context.episode.is_object() && context.episode.value("status",std::string{})!="VERIFIED_SUCCESS")recoveryTransition(id,"FAILED","abandoned");
    session_.reset();
}
Branch3RuntimeResult Branch3Runtime::run(const std::string& goal){return start(goal);}
Branch3RuntimeResult Branch3Runtime::start(const std::string& goal) {
    abandonSession();session_=std::make_unique<Session>(goal);auto& s=*session_;auto& result=s.result;result.blockedStage="Goal decomposition";
    try {
        result.decomposedPlan=decomposer_.decompose(goal);
        if(!result.decomposedPlan.valid) {
            result.message="Goal decomposition failed: "+result.decomposedPlan.message;
            result.cause=result.decomposedPlan.failureCategory==LLMFailureCategory::NONE?RuntimeCause::INVALID_PLAN:dependencyCause(result.decomposedPlan.failureCategory);
            if(result.decomposedPlan.failureCategory!=LLMFailureCategory::NONE)result.outcome=RuntimeOutcome::EXTERNAL_UNAVAILABLE;
            return result;
        }
        result.blockedStage="Workflow graph construction";
        s.kernel=std::make_unique<wf::Kernel>(workflowSteps(result.decomposedPlan));
        if(!s.kernel->graph.valid()){result.message=s.kernel->graph.error();result.cause=RuntimeCause::INVALID_PLAN;return result;}
        result.planningSuccess=true;result.mappedPlan=result.decomposedPlan;result.mappedPlan.steps.clear();
        if(observeEnvironment_)s.environment=Utf8::dump(environmentSnapshot(),"environment");
        recordEvent("[GUIDE] "+ledger(*s.kernel)+"\n");
        return advance();
    }catch(const Utf8::Error& error){result.cause=RuntimeCause::INVALID_UTF8;result.message=error.what();return result;}
    catch(const std::exception& error){result.cause=RuntimeCause::INVALID_PLAN;result.message=error.what();return result;}
}
Branch3RuntimeResult Branch3Runtime::continueSession() {
    if(session_ && (!session_->kernel || !session_->kernel->graph.valid()))return session_->result;
    return resumeSession(true);
}
Branch3RuntimeResult Branch3Runtime::resumeSession(bool refreshBackend) {
    if(!session_){Branch3RuntimeResult result;result.outcome=RuntimeOutcome::NEEDS_USER;result.message="No retained workflow";return result;}
    if(session_->pending)return session_->result;
    if(!session_->kernel){const auto goal=session_->goal;return start(goal);}
    if(!session_->kernel->graph.valid())return session_->result;
    if(session_->kernel->complete())return session_->result;
    session_->result.stateLog.clear();session_->invalidDecisions.clear();session_->noChange=0;session_->staleReferences.clear();
    session_->refreshBeforePlanning=refreshBackend;
    session_->kernel->retry();return advance();
}
Branch3RuntimeResult Branch3Runtime::retryBlockedDependency() {
    if(!session_)return continueSession();
    if(session_->pending && session_->retryAnswer) {const auto answer=*session_->retryAnswer;return answerUser(answer.first,answer.second);}
    if(session_->result.outcome!=RuntimeOutcome::EXTERNAL_UNAVAILABLE && session_->result.outcome!=RuntimeOutcome::RETRYABLE) {
        auto result=session_->result;result.stateLog.clear();result.message="Retry requires an unavailable dependency; use Continue for execution recovery.";return result;
    }
    return resumeSession(false);
}
std::optional<PendingUserDecision> Branch3Runtime::pendingUserDecision() const {
    return session_?session_->pending:std::nullopt;
}
Branch3RuntimeResult Branch3Runtime::requestGuidance() {
    if(session_ && controls().answer)return session_->result;
    if(!controls().guidance) {Branch3RuntimeResult result;if(session_)result=session_->result;result.message="No runnable retained step is available for guidance.";return result;}
    const auto* step=session_->kernel->next();if(!step)return session_->result;
    return askStrategyGuidance(step->id,"evidence_or_strategy_unavailable");
}
void Branch3Runtime::recoveryTransition(const std::string& stepId,const std::string& status,const std::string& outcome) {
    if(!session_)return;auto& context=session_->strategies[stepId];auto& episode=context.episode;
    if(!episode.is_object() || episode.empty())return;
    const auto previous=episode.value("status",std::string{});
    if(previous=="FAILED" || previous=="VERIFIED_SUCCESS" && outcome!="goal_completed")return;
    if(previous==status && outcome!="goal_completed")return;
    episode["status"]=status;episode["outcome"]=outcome;
    episode["event_sequence"]=episode.value("event_sequence",0)+1;
    json after=json::array();const auto offset=episode.value("attempt_start",size_t{});
    for(size_t i=offset;i<context.attempts.size();++i)after.push_back(context.attempts[i]);
    episode["attempts_after_user"]=StrategyRecovery::privateAttempts(after);
    if(outcome=="goal_completed")episode["goal_result"]="COMPLETED";
    else if(status=="FAILED")episode["goal_result"]="NOT_COMPLETED";
    else episode["goal_result"]="IN_PROGRESS";
    if(recoverySink_)try{recoverySink_(episode);}catch(...){recordEvent("[RECOVERY_LOG] Local episode could not be written.\n");}
}
Branch3RuntimeResult Branch3Runtime::askStrategyGuidance(const std::string& stepId,const std::string& blocker) {
    auto& s=*session_;auto& context=s.strategies[stepId];context.active=true;context.blocker=blocker;
    recoveryTransition(stepId,"FAILED","strategy_exhausted");
    auto& result=s.result;result.success=false;
    if(context.questionCount>=recoveryPolicy_.questions) {
        result.outcome=RuntimeOutcome::PARTIAL_SUCCESS;result.cause=RuntimeCause::EVIDENCE_UNAVAILABLE;
        result.message="The bounded strategy and clarification budget is exhausted. Verified progress and evidence are retained.";return result;
    }
    ++context.questionCount;
    PendingUserDecision pending;pending.questionId=s.identity+":"+std::to_string(++s.questionSequence);pending.stepId=stepId;
    pending.reason="STRATEGY_GUIDANCE";pending.workflowState=ledger(*s.kernel);
    pending.allowedActions={"ADD_SEARCH_HINT","ADD_LOCATION_HINT","SUGGEST_STRATEGY","PROVIDE_INFORMATION","USER_CANNOT_CLARIFY"};
    pending.context=context.context(recoveryPolicy_);pending.context["original_goal"]=s.goal;
    pending.context["current_step"]=s.kernel->graph.find(stepId)->intent;
    pending.context["available_primitives"]=json::parse(liveCatalog());
    pending.context["useful_information"]=json::array({"name_fragment","location","time_hint","alternative_method"});
    std::string summary="I tried ";const auto first=context.attempts.size()>3?context.attempts.size()-3:0;
    for(size_t i=first;i<context.attempts.size();++i) {
        if(i>first)summary+=", ";const auto& attempt=context.attempts[i];summary+=attempt.value("strategy",std::string{})+" ("+attempt.value("outcome",std::string{})+")";
        if(attempt.contains("clues"))summary+=" "+Utf8::truncate(attempt["clues"].dump(),200,"clarification.clues");
    }
    if(context.attempts.empty())summary="The available evidence does not establish this step";
    pending.intent=summary+". I still cannot verify the requested outcome. Do you remember another part of the name, another location, or a method I should try?";
    s.pending=pending;s.heldDecision.reset();
    s.kernel->recordFailure(stepId,{RuntimeCause::STRATEGY_GUIDANCE,"strategy_guidance",blocker,true});
    result.outcome=RuntimeOutcome::NEEDS_USER;result.cause=RuntimeCause::STRATEGY_GUIDANCE;result.message=pending.intent;
    result.recovery="Answer this retained question to guide the current step. The original goal and verified progress are preserved.";
    recordEvent("[CLARIFICATION] reason=STRATEGY_GUIDANCE step="+stepId+" attempts="+std::to_string(context.attempts.size())+"\n");return result;
}
Branch3RuntimeResult Branch3Runtime::answerUser(const std::string& questionId,const std::string& answer) {
    if(!session_ || !session_->pending || session_->pending->questionId!=questionId) {
        Branch3RuntimeResult result;if(session_)result=session_->result;
        result.message="No matching active question; the answer was not applied.";return result;
    }
    auto& s=*session_;const auto pending=*s.pending;
    if(answer.size()>8192){s.result.message="Please keep guidance under 8192 bytes; the active question is retained.";return s.result;}
    s.retryAnswer.reset();s.result.outcome=RuntimeOutcome::NEEDS_USER;
    s.result.cause=pending.reason=="STRATEGY_GUIDANCE"?RuntimeCause::STRATEGY_GUIDANCE:pending.reason=="USER_ACTION_REQUIRED"?RuntimeCause::USER_ACTION_REQUIRED:pending.reason=="MISSING_INFORMATION"?RuntimeCause::MISSING_INFORMATION:pending.reason=="TARGET_AMBIGUOUS"?RuntimeCause::TARGET_AMBIGUOUS:RuntimeCause::TARGET_CONFIRMATION;
    s.result.message=pending.intent;
    auto interpret=[&](const std::string& system,const std::string& input) {
        LLMResponse response;
        try {response=answerInterpreter_(system,input);}catch(...) {response={false,"","Answer interpretation connection failed",LLMFailureCategory::CONNECTION_ERROR};}
        auto value=response.success?json::parse(response.content,nullptr,false):json::object();
        if(!response.success || !value.is_object()) {
            const auto failure=response.failureCategory==LLMFailureCategory::NONE?LLMFailureCategory::INVALID_RESPONSE:response.failureCategory;
            s.result.outcome=RuntimeOutcome::EXTERNAL_UNAVAILABLE;s.result.cause=dependencyCause(failure);
            s.result.message="The answer interpreter is unavailable or returned an invalid response. Your question, answer, and workflow are retained; retry the dependency or provide another answer.";
            s.retryAnswer=std::make_pair(questionId,answer);return json::object();
        }
        return value;
    };
    json resolution=json::object();const auto text=FilesystemRanking::normalize(answer);
    if(pending.reason=="USER_ACTION_REQUIRED") {
        if(text=="yes" || text=="y" || text=="allow" || text=="go ahead" || text=="yes go ahead")resolution={{"decision","GRANT_FOREGROUND"}};
        else if(text=="no" || text=="n" || text=="decline")resolution={{"decision","DECLINE_FOREGROUND"}};
        else if(answerInterpreter_)resolution=interpret("Interpret only this foreground-control permission question. Return {decision: GRANT_FOREGROUND} for explicit permission or {decision: DECLINE_FOREGROUND} for refusal, otherwise {decision: UNRESOLVED}. Strategy suggestions alone are not permission.",json{{"question",pending.json()},{"user_answer",answer}}.dump());
    }else if(pending.candidates.empty()) {
        if(text.empty())return s.result;
        if(text=="i don t know" || text=="no idea" || text=="i cannot clarify")resolution={{"decision","USER_CANNOT_CLARIFY"}};
        else if(answerInterpreter_)try {
            auto context=pending.context;context["original_goal"]=s.goal;context["current_step"]=s.kernel->graph.find(pending.stepId)->intent;
            context["available_primitives"]=json::parse(liveCatalog());context["current_evidence"]=ledger(*s.kernel);
            context["strategy_context"]=s.strategies[pending.stepId].context(recoveryPolicy_);
            resolution=interpret(
                "Interpret free-form guidance for the retained question and original goal only. Return strict JSON. Allowed decisions: ADD_SEARCH_HINT with hint {type: name_contains|extension|modified_within_days|query_concept, value: string or numeric age}; ADD_LOCATION_HINT with value string; SUGGEST_STRATEGY with strategy (short identifier) and required_primitives (nonempty list of exact registry names); PROVIDE_INFORMATION with information string; USER_CANNOT_CLARIFY. Never invent primitives, execute an operation, change postconditions, authorize global input, or invent candidate IDs. Explain an unavailable method through its required unsupported primitive, not a fictitious available capability. A strategy is advice for planning, never an executable plan.",
                json{{"question",pending.json()},{"context",context},{"user_answer",answer}}.dump());
        }catch(...){}
        else if(pending.reason=="MISSING_INFORMATION")resolution={{"decision","PROVIDE_INFORMATION"},{"information",answer}};
    }else {
        size_t nameMatches=0;json nameSelection;
        for(size_t i=0;i<pending.candidates.size();++i) {
            const auto& row=pending.candidates[i];
            if(text==FilesystemRanking::normalize(row.value("name",std::string{}))){++nameMatches;nameSelection={{"decision","SELECT_CANDIDATE"},{"candidate_id",row["candidate_id"]}};}
            if(answer==std::to_string(i+1) || answer==row.value("candidate_id",std::string{}))
                resolution={{"decision","SELECT_CANDIDATE"},{"candidate_id",row["candidate_id"]}};
        }
        if(resolution.empty() && nameMatches==1)resolution=nameSelection;
        if(resolution.empty() && nameMatches>1)return s.result;
        if((text=="yes" || text=="that s the one" || text=="yes that s it" || text=="that is the one") && pending.reason=="TARGET_CONFIRMATION")
            resolution={{"decision","SELECT_CANDIDATE"},{"candidate_id",pending.preferredCandidateId.empty()?pending.candidates[0]["candidate_id"]:json(pending.preferredCandidateId)}};
        if((text=="yes" || text=="that s the one" || text=="yes that s it" || text=="that is the one") && pending.reason=="TARGET_AMBIGUOUS")return s.result;
        if(text=="the second one" && pending.candidates.size()>1)resolution={{"decision","SELECT_CANDIDATE"},{"candidate_id",pending.candidates[1]["candidate_id"]}};
        if(text=="show me the others" || text=="show other matches" || text=="o")resolution={{"decision","SHOW_CANDIDATES"}};
        if(text=="search again" || text=="no search again" || text=="s")resolution={{"decision","REFINE_SEARCH"}};
        if(resolution.empty() && answerInterpreter_) {
            try {
                resolution=interpret(
                    "Map the answer to this active host question only. Return strict JSON. Allowed decisions: SELECT_CANDIDATE with candidate_id from supplied candidates, SHOW_CANDIDATES, REFINE_SEARCH with optional query, extension, modified_within_days, or UNRESOLVED. Never invent a candidate or operation. Ambiguous yes cannot select among multiple plausible candidates. Modification time is not download time.",
                    json{{"question",pending.json()},{"user_answer",answer}}.dump());
            }catch(const std::exception&) {resolution=json::object();}
        }
    }
    if(!resolution.is_object() || !resolution.contains("decision") || !resolution["decision"].is_string())return s.result;
    const auto action=resolution["decision"].get<std::string>();
    if(std::find(pending.allowedActions.begin(),pending.allowedActions.end(),action)==pending.allowedActions.end())return s.result;
    if(action=="SHOW_CANDIDATES")return s.result;
    auto beginEpisode=[&](const json& guidance) {
        auto& context=s.strategies[pending.stepId];
        recoveryTransition(pending.stepId,"FAILED","superseded_guidance");
        json capabilities=json::array();for(const auto& [name,definition]:PrimitiveRegistry::getCatalog())if(capabilities.size()<64)capabilities.push_back(name);
        context.episode={{"capability_context",{{"registered_primitives",capabilities}}},{"schema_version",1},{"episode_id",s.identity+":episode:"+std::to_string(++s.episodeSequence)},
            {"step_id",pending.stepId},{"goal_class",context.goalClass},{"initial_problem",context.blocker},
            {"attempts_before_user",StrategyRecovery::privateAttempts(context.attempts)},{"attempt_start",context.attempts.size()},
            {"clarification_reason",pending.reason},{"user_guidance",StrategyRecovery::privateGuidance(guidance)}};
        recoveryTransition(pending.stepId,"USER_SUGGESTED","guidance_received");
    };
    if(action=="USER_CANNOT_CLARIFY") {
        recoveryTransition(pending.stepId,"FAILED","user_cannot_clarify");
        s.result.message="No additional guidance was applied. The same workflow is retained; you can answer this question later, choose V, or quit.";return s.result;
    }
    if(action=="GRANT_FOREGROUND") {
        const std::set<std::string> explicitConsent={"yes","y","allow","go ahead","yes go ahead","yes you can","yes take control","sure","sure go ahead"};
        if(!explicitConsent.contains(text)){s.result.message="Please answer yes or no to explicitly authorize foreground control; no input was sent.";return s.result;}
        s.strategies[pending.stepId].foregroundAllowed=true;
        if(s.heldDecision)s.resumeDecision=s.heldDecision;
    }else if(action=="DECLINE_FOREGROUND") {
        recoveryTransition(pending.stepId,"FAILED","foreground_permission_declined");s.heldDecision.reset();s.pending.reset();
        return askStrategyGuidance(pending.stepId,"foreground_permission_declined");
    }else if(action=="ADD_SEARCH_HINT" || action=="ADD_LOCATION_HINT" || action=="SUGGEST_STRATEGY" || action=="PROVIDE_INFORMATION") {
        auto& context=s.strategies[pending.stepId];
        if(context.attempts.size()>=recoveryPolicy_.totalAttempts || context.hints.size()>=8) {
            s.result.message="The retained recovery budget is exhausted; more guidance cannot start further attempts.";return s.result;
        }
        bool valid=true,unavailable=false,foreground=false;
        std::set<std::string> keys={"decision"};
        if(action=="SUGGEST_STRATEGY") {
            keys.insert("strategy");keys.insert("required_primitives");
            valid=resolution.contains("strategy") && resolution["strategy"].is_string() && StrategyRecovery::identifier(resolution["strategy"].get<std::string>()) && resolution.contains("required_primitives") && resolution["required_primitives"].is_array() && !resolution["required_primitives"].empty() && resolution["required_primitives"].size()<=8;
            if(valid)for(const auto& name:resolution["required_primitives"]) {
                if(!name.is_string() || !StrategyRecovery::identifier(name.get<std::string>())){valid=false;break;}
                if(!PrimitiveRegistry::isSupported(name.get<std::string>()))unavailable=true;
                foreground |= StrategyRecovery::foreground(name.get<std::string>());
            }
        }else if(action=="ADD_SEARCH_HINT") {
            keys.insert("hint");valid=resolution.contains("hint") && resolution["hint"].is_object();
            if(valid){const auto& hint=resolution["hint"];valid=hint.size()==2 && hint.contains("type") && hint["type"].is_string() && hint.contains("value");
                if(valid){const auto type=hint["type"].get<std::string>();valid=type=="name_contains" || type=="extension" || type=="modified_within_days" || type=="query_concept";
                    if(type=="modified_within_days")valid=hint["value"].is_number() && std::isfinite(hint["value"].get<double>()) && hint["value"].get<double>()>=0 && hint["value"].get<double>()<=36500;
                    else valid &= hint["value"].is_string() && !hint["value"].get<std::string>().empty() && hint["value"].get<std::string>().size()<=1024;}}
        }else {const auto field=action=="ADD_LOCATION_HINT"?"value":"information";keys.insert(field);valid=resolution.contains(field) && resolution[field].is_string() && !resolution[field].get<std::string>().empty() && resolution[field].get<std::string>().size()<=4096;}
        for(const auto& [key,value]:resolution.items())if(!keys.contains(key))valid=false;
        if(!valid){s.result.message="That answer did not produce valid structured guidance; the active question is retained.";return s.result;}
        if(unavailable) {
            beginEpisode({{"decision",action}});recoveryTransition(pending.stepId,"FAILED","unsupported_capability");
            s.result.message="The suggested method requires an unregistered capability. It was not executed. Suggest a method using the available registry or provide another clue.";return s.result;
        }
        beginEpisode(resolution);context.active=true;context.guided=true;context.sinceGuidance=0;context.rejectedPlans=0;
        context.foregroundAllowed=false;context.foregroundSuggested=foreground;context.hints.push_back(resolution);
        s.state.addHistory("Structured user guidance added to the current step strategy context. This is a hint, not evidence or permission to change the declared postcondition.");
    }else

    if(action=="SELECT_CANDIDATE") {
        if(!resolution.contains("candidate_id") || !resolution["candidate_id"].is_string())return s.result;
        const auto found=std::find_if(pending.candidates.begin(),pending.candidates.end(),[&](const auto& row){return row["candidate_id"]==resolution["candidate_id"];});
        if(found==pending.candidates.end())return s.result;
        const auto* search=s.kernel->facts.find(pending.searchEvidenceId);
        if(!search || !search->current || !search->facts.contains("results") || std::find(search->facts["results"].begin(),search->facts["results"].end(),*found)==search->facts["results"].end())return s.result;
        s.resolvedTarget=found->value("path",std::string{});s.confirmedTargets.insert(s.resolvedTarget);
        wf::Evidence evidence;evidence.stepId=pending.stepId;evidence.operation="user.select_candidate";
        evidence.facts={{"resolved_target",s.resolvedTarget},{"path",s.resolvedTarget},{"candidate_id",resolution["candidate_id"]},
            {"search_evidence_id",pending.searchEvidenceId},{"question_id",questionId},{"user_confirmed",true}};
        s.kernel->facts.add(std::move(evidence));
        if(s.heldDecision) {
            auto decision=*s.heldDecision;auto operation=json::parse(decision.operationJson);
            // Substitute only the original inferred identity, never arbitrary text or unrelated operands.
            for(const auto& field:pending.targetFields)operation["parameters"][field]=s.resolvedTarget;
            decision.operationJson=operation.dump();s.resumeDecision=decision;
        }
        s.state.addHistory("User selected structured canonical target; use resolved_target from evidence, do not search again.");
    }else if(action=="REFINE_SEARCH") {
        const auto search=s.searchDecisions.find(pending.searchEvidenceId);
        if(search==s.searchDecisions.end())return s.result;
        auto decision=search->second;decision.stepId=pending.stepId;auto operation=json::parse(decision.operationJson);
        bool changed=false;
        for(const auto key:{"query","extension","modified_within_days"})if(resolution.contains(key)) {
            if(!resolution[key].is_string() && !(std::string(key)=="modified_within_days" && resolution[key].is_number_integer()))return s.result;
            operation["parameters"][key]=resolution[key];changed=true;
        }
        if(!changed && text!="search again" && text!="s" && text!="no search again")return s.result;
        decision.operationJson=operation.dump();s.resumeDecision=decision;
        s.resolvedTarget.clear();
    }else return s.result;
    s.pending.reset();s.heldDecision.reset();s.result.answer.clear();s.result.recovery.clear();s.result.stateLog.clear();
    s.kernel->retry();return advance();
}
Branch3RuntimeResult Branch3Runtime::advance() {
    try{return advanceImpl();}
    catch(const Utf8::Error& error){auto& r=session_->result;r.success=false;r.outcome=RuntimeOutcome::FAILED;r.cause=RuntimeCause::INVALID_UTF8;r.message=error.what();return r;}
    catch(const std::exception& error){auto& r=session_->result;r.success=false;r.outcome=RuntimeOutcome::FAILED;r.cause=RuntimeCause::VALIDATION_ERROR;r.message=error.what();return r;}
}
Branch3RuntimeResult Branch3Runtime::advanceImpl() {
    auto& s=*session_;auto& result=s.result;auto& kernel=*s.kernel;
    result.blockedStage="Primitive catalog serialization";const auto catalog=liveCatalog();
    auto execute=[&](const PrimitiveOperation& operation) {
        try{return executor_(operation);}
        catch(const std::exception& error) {
            ExecutionResult failure{};failure.action=operation.name;failure.status=ExecutionStatus::FAILED;failure.reason=ExecutionFailureReason::EXECUTION_ERROR;
            failure.failureCategory=dynamic_cast<const Utf8::Error*>(&error)?"invalid_utf8":"execution_error";failure.message=error.what();return failure;
        }
    };
    auto finish=[&](RuntimeOutcome outcome,RuntimeCause cause,std::string message) {
        result.outcome=outcome;result.cause=cause;result.success=outcome==RuntimeOutcome::COMPLETED;result.message=std::move(message);
        if(result.success){result.blockedStage.clear();result.lastExecutionIssue.clear();result.recovery.clear();if(result.answer.empty())result.answer=result.message;
            for(const auto& [id,context]:s.strategies)if(context.episode.is_object() && context.episode.value("status",std::string{})=="VERIFIED_SUCCESS")recoveryTransition(id,"VERIFIED_SUCCESS","goal_completed");
        }else if(outcome==RuntimeOutcome::FAILED || outcome==RuntimeOutcome::PARTIAL_SUCCESS && cause==RuntimeCause::EVIDENCE_UNAVAILABLE) {
            for(const auto& [id,context]:s.strategies)if(context.episode.is_object() && (context.episode.value("status",std::string{})=="ATTEMPTED" || context.episode.value("status",std::string{})=="USER_SUGGESTED"))recoveryTransition(id,"FAILED","verification_unavailable");
        }
        return result;
    };
    auto announce=[&]() {
        for(const auto& step:kernel.graph.steps())if((step.status==wf::StepStatus::VERIFIED || step.status==wf::StepStatus::SKIPPED) && s.announced.insert(step.id).second) {
            const auto text=step.id+": "+wf::statusName(step.status)+" — "+step.intent;
            auto& context=s.strategies[step.id];
            if(step.status==wf::StepStatus::VERIFIED && context.episode.is_object()) {
                if(context.episode.value("status",std::string{})=="USER_SUGGESTED")recoveryTransition(step.id,"ATTEMPTED","existing_evidence_evaluation");
                if(context.episode.value("status",std::string{})=="ATTEMPTED")recoveryTransition(step.id,"VERIFIED_SUCCESS","step_verified");
            }
            context.active=false;
            result.verifiedProgress.push_back(text);recordEvent(step.status==wf::StepStatus::SKIPPED?"[NOT_APPLICABLE] "+text+"\n":"[VERIFIED] "+text+"\n");
        }
    };
    auto hostEvidence=[&](const std::string& id,const PrimitiveOperation& operation,const ExecutionResult& execution) {
        auto evidence=executionEvidence(id,operation,execution);
        if(operation.name.starts_with("application."))evidence.facts["runtime_session"]=s.identity;
        if(operation.name=="application.observe" && execution.status==ExecutionStatus::SUCCESS) {
            std::vector<const wf::Evidence*> candidates,dispatches;
            for(const auto& prior:kernel.facts.all())if(prior.category==wf::EvidenceCategory::APPLICATION_DISPATCH) {
                dispatches.push_back(&prior);const auto* producer=kernel.graph.find(prior.stepId);
                if(prior.stepId==id || producer && producer->status==wf::StepStatus::VERIFIED && kernel.graph.ancestor(prior.stepId,id))candidates.push_back(&prior);
            }
            evidence.facts=ApplicationCorrelation::enrich(std::move(evidence.facts),candidates,dispatches);
            recordEvent("[APPLICATION_CORRELATION] "+ApplicationCorrelation::diagnostic(evidence.facts,candidates).dump()+"\n");
        }
        return evidence;
    };
    enum class DeclaredState { WAITING_FOR_OPERATION, VERIFIED, EVIDENCE_UNAVAILABLE };
    auto evaluateDeclared=[&](const std::string& id) {
        const auto* step=kernel.graph.find(id);
        if(!step || step->postconditions.size()==1 && step->postconditions[0].kind==wf::PredicateKind::INTERPRETED)return DeclaredState::WAITING_FOR_OPERATION;
        if(step->status==wf::StepStatus::VERIFIED)return DeclaredState::VERIFIED;
        std::vector<std::string> ids;for(const auto& evidence:kernel.facts.all())ids.push_back(evidence.id);
        std::string error;
        if(kernel.verify(id,ids,{},error)) {
            recordEvent("[VERIFICATION] origin=runtime contract=declared step="+id+" VERIFIED\n");
            if(s.resumeDecision && s.resumeDecision->stepId==id)s.resumeDecision.reset();
            return DeclaredState::VERIFIED;
        }
        // An operation can still establish the intent. Once this step has a
        // relevant observation, its failed contract needs evidence, not prose.
        const bool acted=std::any_of(result.operationResults.begin(),result.operationResults.end(),[&](const auto& op){return op.stepId==id;});
        bool observed=false;
        if(acted)for(const auto& post:step->postconditions)if(post.kind==wf::PredicateKind::SEMANTIC) {
            const auto source=post.value.value("source",std::string{}),kind=PredicateRegistry::kindOf(post.value);
            for(const auto& evidence:kernel.facts.all())if(evidence.current && evidence.stepId==id && evidence.category!=wf::EvidenceCategory::FAILURE) {
                observed |= source=="filesystem" && evidence.operation==(kind.starts_with("search_")?"filesystem.search":"filesystem.inspect") ||
                    source=="browser" && evidence.category==wf::EvidenceCategory::BROWSER_OBSERVATION ||
                    source=="application" && evidence.category==wf::EvidenceCategory::APPLICATION_OBSERVATION;
            }
        }
        if(!observed)return DeclaredState::WAITING_FOR_OPERATION;
        auto& recovery=s.strategies[id];
        bool emptySearch=false;
        for(const auto& fact:kernel.facts.all())if(fact.current && fact.stepId==id && fact.operation=="filesystem.search" && fact.facts.contains("results") && fact.facts["results"].is_array())emptySearch=fact.facts["results"].empty();
        if(recovery.active && recovery.goalClass=="filesystem_discovery" && emptySearch) {
            if(recovery.canTry(recoveryPolicy_))return DeclaredState::WAITING_FOR_OPERATION;
            askStrategyGuidance(id,"no_confident_search_candidate");return DeclaredState::EVIDENCE_UNAVAILABLE;
        }
        kernel.recordFailure(id,{RuntimeCause::EVIDENCE_UNAVAILABLE,"declared_postcondition_unmet",error,true});
        result.recovery="Supply new evidence for the original declared contract; planner restatements cannot change it.";
        recordEvent("[VERIFICATION] origin=runtime contract=declared step="+id+" EVIDENCE_UNAVAILABLE "+error+"\n");
        finish(RuntimeOutcome::PARTIAL_SUCCESS,RuntimeCause::EVIDENCE_UNAVAILABLE,error);
        return DeclaredState::EVIDENCE_UNAVAILABLE;
    };
    const unsigned budget=std::clamp(static_cast<unsigned>(kernel.graph.steps().size()*6),18U,128U);
    for(unsigned attempt=0;attempt<budget;++attempt) {
        const auto* selected=kernel.next();announce();
        if(kernel.complete())return finish(RuntimeOutcome::COMPLETED,RuntimeCause::NONE,"All required workflow steps are verified or validly skipped.");
        if(!selected) {
            auto cause=RuntimeCause::INVALID_PLAN;RuntimeOutcome outcome=RuntimeOutcome::NEEDS_USER;
            for(const auto& step:kernel.graph.steps())if(step.recovery.active){cause=step.recovery.active->category;break;}
            return finish(outcome,cause,wf::WorkflowScheduler::blocker(kernel.graph));
        }
        const auto id=selected->id;const auto& source=originalStep(result.decomposedPlan,id);
        std::string boundArtifactParent;
        if(source.artifactParentReference) {
            const auto binding=ArtifactBinding::parent(kernel.graph,kernel.facts,id,source.artifactParentReference->sourceStepId);
            if(!binding.okay) {
                kernel.recordFailure(id,{RuntimeCause::EVIDENCE_UNAVAILABLE,"artifact_parent_unavailable",binding.error,true});
                return finish(RuntimeOutcome::PARTIAL_SUCCESS,RuntimeCause::EVIDENCE_UNAVAILABLE,binding.error);
            }
            boundArtifactParent=binding.path;
        }
        result.blockedStage=id+": "+selected->intent;s.state.nextIteration();
        const bool refreshBrowserEvidence=s.refreshBeforePlanning && s.browserActive && !s.applicationScopes.contains(id) && result.cause==RuntimeCause::EVIDENCE_UNAVAILABLE;
        if(s.refreshBeforePlanning) {
            s.refreshBeforePlanning=false;
            const auto scope=s.applicationScopes.find(id);
            if(scope!=s.applicationScopes.end()) {
                const bool evidenceBlocked=result.cause==RuntimeCause::EVIDENCE_UNAVAILABLE;
                const auto operation=scope->second;
                recordEvent("[REFRESH] step="+id+" application.observe before planner\n");
                auto execution=execute(operation);
                result.operationResults.push_back({id,0,execution});
                auto evidence=hostEvidence(id,operation,execution);
                const auto facts=evidence.facts;
                const auto evidenceId=kernel.facts.add(std::move(evidence)).id;
                if(execution.status!=ExecutionStatus::SUCCESS) {
                    if(evidenceBlocked)return finish(RuntimeOutcome::PARTIAL_SUCCESS,RuntimeCause::EVIDENCE_UNAVAILABLE,
                        "Backend refresh failed; the original application evidence limitation remains: "+execution.message);
                    return finish(RuntimeOutcome::PARTIAL_SUCCESS,backendCause(execution.failureCategory),execution.message);
                }
                const auto fingerprint=ApplicationState::fingerprint(facts);
                const bool unchanged=s.lastApplicationFingerprint.contains(id) && s.lastApplicationFingerprint[id]==fingerprint;
                s.lastApplicationFingerprint[id]=fingerprint;
                s.applicationFingerprints[id+":"+operationKey(operation,json::object())]=fingerprint;
                recordEvent("[EXECUTION] application.observe : SUCCESS evidence="+evidenceId+" proof="+facts.value("target_proof_level",std::string("unavailable"))+"\n");
                if(evidenceBlocked && unchanged)return finish(RuntimeOutcome::PARTIAL_SUCCESS,RuntimeCause::EVIDENCE_UNAVAILABLE,
                    "Fresh application observation is semantically unchanged. The original evidence limitation remains; no planner call or mutation replay was made.");
                if(evidenceBlocked && ApplicationState::targetUnavailable(facts))return finish(RuntimeOutcome::PARTIAL_SUCCESS,RuntimeCause::EVIDENCE_UNAVAILABLE,
                    "Fresh application observation still lacks exact or sufficient correlated target evidence; no planner call was made.");
                s.state.addHistory("Backend refreshed before planning; structured evidence="+evidenceId);
            }else if(result.cause==RuntimeCause::EVIDENCE_UNAVAILABLE && !s.browserActive) {
                return finish(RuntimeOutcome::PARTIAL_SUCCESS,RuntimeCause::EVIDENCE_UNAVAILABLE,
                    "No securely scoped application refresh is available. The original evidence limitation remains; no planner call or mutation replay was made.");
            }else if(observeEnvironment_)s.environment=Utf8::dump(environmentSnapshot(),"environment.refresh");
        }
        if(s.browserActive) {
            PrimitiveOperation observe{};observe.name="browser.observe";observe.type=PrimitiveOperationType::BROWSER_OBSERVE;observe.resolved=true;
            const auto observation=execute(observe);
            if(observation.status!=ExecutionStatus::SUCCESS) {
                if(refreshBrowserEvidence)return finish(RuntimeOutcome::PARTIAL_SUCCESS,RuntimeCause::EVIDENCE_UNAVAILABLE,
                    "Browser refresh failed; the original evidence limitation remains: "+observation.message);
                kernel.recordFailure(id,{backendCause(observation.failureCategory),observation.failureCategory,observation.message,true});
                return finish(RuntimeOutcome::EXTERNAL_UNAVAILABLE,backendCause(observation.failureCategory),observation.message);
            }
            const auto raw=observation.browserData.empty()?observation.message:observation.browserData;
            s.state.setObservation(Utf8::dump(SemanticEvidence::plannerBrowser(BrowserState::snapshot(raw),selected->intent+" "+s.goal),"planner.browser"));
            auto evidence=SemanticEvidence::browserObservation(id,raw);
            if(evidence.facts.value("auth_required",false))return finish(RuntimeOutcome::NEEDS_USER,RuntimeCause::AUTH_REQUIRED,"Complete authentication in the controlled browser, then continue.");
            const auto fingerprint=BrowserState::fingerprint(raw);
            const bool unchanged=fingerprint==s.lastFingerprint;
            s.lastFingerprint=fingerprint;
            if(!s.lastBrowserAction.empty()){s.noChange=fingerprint==s.beforeAction?s.noChange+1:0;s.lastBrowserAction.clear();}
            kernel.facts.add(std::move(evidence));
            if(refreshBrowserEvidence && unchanged)return finish(RuntimeOutcome::PARTIAL_SUCCESS,RuntimeCause::EVIDENCE_UNAVAILABLE,
                "Fresh browser observation is semantically unchanged. The original evidence limitation remains; no planner call or mutation replay was made.");
        }else s.state.setObservation(s.environment);
        const auto declared=evaluateDeclared(id);
        if(declared==DeclaredState::VERIFIED){announce();continue;}
        if(declared==DeclaredState::EVIDENCE_UNAVAILABLE)return result;
        if(s.strategies[id].active && !s.strategies[id].canTry(recoveryPolicy_))return askStrategyGuidance(id,"strategy_budget_exhausted");
        auto decision=s.resumeDecision?*s.resumeDecision:planner_.choose(s.goal,result.decomposedPlan,s.state.render()+ledger(kernel)+"\nSTRATEGY_RECOVERY_CONTEXT:\n"+Utf8::dump(s.strategies[id].context(recoveryPolicy_),"strategy.context"),catalog,id);
        s.resumeDecision.reset();
        if(transient(decision.failureCategory))decision=planner_.choose(s.goal,result.decomposedPlan,s.state.render()+ledger(kernel)+"\nSTRATEGY_RECOVERY_CONTEXT:\n"+Utf8::dump(s.strategies[id].context(recoveryPolicy_),"strategy.context"),catalog,id);
        if(decision.failureCategory!=LLMFailureCategory::NONE) {
            kernel.recordFailure(id,{dependencyCause(decision.failureCategory),llmFailureName(decision.failureCategory),decision.message,true});
            return finish(RuntimeOutcome::EXTERNAL_UNAVAILABLE,dependencyCause(decision.failureCategory),decision.message);
        }
        recordEvent("[PLANNER] ready="+id+" "+decision.status+": "+BrowserState::summary(decision.message)+"\n");
        auto reject=[&](const std::string& message) {
            s.state.addHistory("Rejected decision for runtime-selected READY step "+id+": "+message);recordEvent("[VALIDATION] "+message+"\n");
            if(++s.invalidDecisions[id]>=3){kernel.recordFailure(id,{RuntimeCause::VALIDATION_ERROR,"invalid_planner_decision",message,true});return true;}return false;
        };
        if(decision.status=="ask_user" || decision.status=="blocked") {
            auto& recovery=s.strategies[id];
            if(recovery.active) {
                if(recovery.attempts.size()<2 && recovery.canTry(recoveryPolicy_) && ++recovery.rejectedPlans<2) {
                    s.state.addHistory("One safe strategy failed. Try a materially different registered read-only operation before asking for guidance, if useful; do not restate satisfaction.");continue;
                }
                return askStrategyGuidance(id,recovery.blocker);
            }
            if(recovery.attempts.size()>=2)return askStrategyGuidance(id,"planner_requested_guidance");
            PendingUserDecision pending;pending.questionId=s.identity+":"+std::to_string(++s.questionSequence);pending.stepId=id;
            pending.reason="MISSING_INFORMATION";pending.intent="Which target, location, time constraint, or method should I use for this step: "+selected->intent+"?";pending.allowedActions={"PROVIDE_INFORMATION","ADD_SEARCH_HINT","ADD_LOCATION_HINT","SUGGEST_STRATEGY","USER_CANNOT_CLARIFY"};pending.workflowState=ledger(kernel);pending.context=s.strategies[id].context(recoveryPolicy_);s.pending=pending;
            kernel.recordFailure(id,{RuntimeCause::MISSING_INFORMATION,decision.status,"missing_information",true});return finish(RuntimeOutcome::NEEDS_USER,RuntimeCause::MISSING_INFORMATION,pending.intent);
        }
        if(decision.stepId!=id) {
            if(reject("Planner must advance only "+id+". Scheduling and whole-goal completion are owned by the kernel."))return finish(RuntimeOutcome::PARTIAL_SUCCESS,RuntimeCause::VALIDATION_ERROR,"Planner repeatedly attempted to override workflow scheduling.");
            continue;
        }
        if(decision.status=="satisfied") {
            std::vector<std::string> ids;for(const auto& evidence:kernel.facts.all())ids.push_back(evidence.id);
            std::string error;
            const bool interpreted=selected->postconditions.size()==1 && selected->postconditions[0].kind==wf::PredicateKind::INTERPRETED;
            const auto predicates=interpreted?interpretation(decision,error):std::vector<wf::Postcondition>{};
            recordEvent("[VERIFICATION_INPUT] origin=planner step="+id+" contract="+(interpreted?"observed_interpretation":"declared")+" received="+decision.verificationJson+"\n");
            if(interpreted && error.starts_with("Unsupported verification predicate") && s.proofSchemaRepairs.insert(id).second) {
                s.state.addHistory("Planner verification schema repair (one attempt): "+error+". Use CANONICAL_POSTCONDITION_REGISTRY and retained structured facts. For a search use filesystem.search_result_matches with the executed search query, root, type, extension and recency constraints, never execution success. Do not execute or search again to repair proof syntax.");
                recordEvent("[VERIFICATION_REPAIR] origin=planner step="+id+" "+error+"\n");continue;
            }
            if(!error.empty() || !kernel.verify(id,ids,predicates,error)) {
                if(s.strategies[id].active)return askStrategyGuidance(id,s.strategies[id].blocker);
                if(!interpreted) {
                    kernel.recordFailure(id,{RuntimeCause::EVIDENCE_UNAVAILABLE,"declared_postcondition_unmet",error,true});
                    return finish(RuntimeOutcome::PARTIAL_SUCCESS,RuntimeCause::EVIDENCE_UNAVAILABLE,error);
                }
                const auto signature=decision.verificationJson+ledger(kernel);
                const bool repeated=s.rejectedProofs[id]==signature;s.rejectedProofs[id]=signature;
                if(repeated || error.starts_with("Unsupported verification predicate")) {
                    const auto cause=error.starts_with("Unsupported verification predicate")?RuntimeCause::UNSUPPORTED_VERIFICATION:RuntimeCause::EVIDENCE_UNAVAILABLE;
                    kernel.recordFailure(id,{cause,"semantic_proof_unavailable",error,true});return finish(RuntimeOutcome::PARTIAL_SUCCESS,cause,error);
                }
                if(reject(error+". Supply structured verification of this READY step using the fact store; log prose is not evidence."))return finish(RuntimeOutcome::PARTIAL_SUCCESS,RuntimeCause::VALIDATION_ERROR,error);
                continue;
            }
            recordEvent("[VERIFICATION] "+id+" "+(interpreted?decision.verificationJson:Utf8::dump(selected->postconditions[0].value,"declared.verification"))+"\n");
            if(!decision.answer.empty())result.answer=decision.answer;else if(!decision.message.empty())result.answer=decision.message;
            s.invalidDecisions.erase(id);announce();continue;
        }
        if(decision.status!="operation") {
            if(reject("Return an operation or structured satisfaction for the selected step. done cannot complete a workflow."))return finish(RuntimeOutcome::PARTIAL_SUCCESS,RuntimeCause::VALIDATION_ERROR,"Planner returned no step advancement.");continue;
        }
        auto proposed=json::parse(decision.operationJson,nullptr,false);
        if(proposed.is_object() && proposed.contains("parameters") && proposed["parameters"].is_object()) {
            for(auto& [field,value]:proposed["parameters"].items())if(value.is_string() && (field=="target" || field=="source" || field=="destination" || field=="location" || field=="application")) {
                if(value=="$resolved_target" && !s.resolvedTarget.empty())value=s.resolvedTarget;
                else if(value=="$resolved_parent" && !s.resolvedTarget.empty())value=Utf8::path(Utf8::toPath(s.resolvedTarget).parent_path());
            }
            const auto operationName=proposed.value("name",std::string{});
            if(source.artifactParentReference && (operationName=="filesystem.create" || operationName=="filesystem.create_file"))proposed["parameters"]["location"]=boundArtifactParent;
            decision.operationJson=proposed.dump();
        }
        if(proposed.is_object() && proposed.contains("name") && proposed["name"].is_string() && !PrimitiveRegistry::isSupported(proposed["name"].get<std::string>())) {
            const auto message="Unsupported capability: "+proposed["name"].get<std::string>()+" is absent from the canonical primitive registry.";
            if(s.strategies[id].active)return askStrategyGuidance(id,"unsupported_strategy");
            kernel.recordFailure(id,{RuntimeCause::UNSUPPORTED_CAPABILITY,"unsupported_capability",message,false});
            return finish(RuntimeOutcome::NEEDS_USER,RuntimeCause::UNSUPPORTED_CAPABILITY,message);
        }
        auto mapped=mapper_.mapStep(source,decision.operationJson);
        if(!mapped.valid || mapped.steps.empty() || mapped.steps.front().operations.size()!=1) {
            if(reject(mapped.message))return finish(RuntimeOutcome::PARTIAL_SUCCESS,RuntimeCause::VALIDATION_ERROR,mapped.message);continue;
        }
        const auto& operation=mapped.steps.front().operations.front();
        // Every inferred filesystem identity remains unconfirmed until the user selects it.
        // Read-only observations and opening a containing directory are harmless.
        if(!readOnly(operation) && operation.name!="filesystem.open_directory") {
            const auto parameters=proposed.value("parameters",json::object());
            for(auto search=kernel.facts.all().rbegin();search!=kernel.facts.all().rend();++search) {
                const auto* producer=kernel.graph.find(search->stepId);
                if(!search->current || search->operation!="filesystem.search" || !search->facts.value("operation_succeeded",false) ||
                   !(search->stepId==id || producer && producer->status==wf::StepStatus::VERIFIED && kernel.graph.ancestor(search->stepId,id)))continue;
                for(const auto& row:search->facts.value("results",json::array())) {
                const auto path=row.value("path",std::string{});bool uses=false;std::vector<std::string> targetFields;
                for(const auto field:{"target","source","destination","location","application","content"})
                    if(parameters.contains(field) && parameters[field].is_string()) {
                        const auto value=parameters[field].get<std::string>();
                        if(wf::sameIdentity(value,path) || wf::sameIdentity(value,Utf8::path(Utf8::toPath(path).filename()))) {uses=true;targetFields.push_back(field);}
                        else if(!path.empty() && value.find(path)!=std::string::npos)uses=true;
                    }
                if(!uses)continue;
                const auto choices=discoveryCandidates(*search,producer);
                if(std::find(choices.begin(),choices.end(),row)==choices.end())return finish(RuntimeOutcome::NEEDS_USER,RuntimeCause::VALIDATION_ERROR,"Proposed inferred target violates the declared discovery constraints.");
                if(s.confirmedTargets.contains(path))continue;
                // Exact canonical user paths carry their own identity authorization.
                if(!path.empty() && s.goal.find(path)!=std::string::npos)continue;
                PendingUserDecision pending;pending.questionId=s.identity+":"+std::to_string(++s.questionSequence);pending.stepId=id;
                pending.reason="TARGET_CONFIRMATION";pending.intent="Is this the file you meant? Select a candidate before "+operation.name+".";
                pending.candidates=choices;pending.searchEvidenceId=search->id;pending.preferredCandidateId=row.value("candidate_id",std::string{});
                pending.allowedActions={"SELECT_CANDIDATE","SHOW_CANDIDATES","REFINE_SEARCH"};pending.workflowState=ledger(kernel);
                pending.targetFields=targetFields;s.pending=pending;
                if(!targetFields.empty())s.heldDecision=decision;else s.heldDecision.reset();
                kernel.recordFailure(id,{RuntimeCause::TARGET_CONFIRMATION,"target_confirmation",pending.intent,true});
                return finish(RuntimeOutcome::NEEDS_USER,RuntimeCause::TARGET_CONFIRMATION,pending.intent);
                }
            }
        }
        const auto key=operationKey(operation,proposed);
        auto& recovery=s.strategies[id];
        auto strategyIdentity=key;
        if(operation.name=="filesystem.search") {
            auto location=operation.parameters.location;std::replace(location.begin(),location.end(),'\\','/');
            std::transform(location.begin(),location.end(),location.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
            while(location.size()>1 && location.back()=='/')location.pop_back();
            auto extension=operation.parameters.extension;if(!extension.empty() && extension.front()!='.')extension.insert(extension.begin(),'.');
            std::transform(extension.begin(),extension.end(),extension.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
            const auto tokens=FilesystemRanking::tokens(operation.parameters.query);
            strategyIdentity=operation.name+json{{"location",location},{"query_tokens",tokens},{"extension",extension},
                {"entry_type",operation.parameters.entryType.empty()?"any":operation.parameters.entryType},
                {"max_results",operation.parameters.maxResults.empty()?"100":operation.parameters.maxResults},{"modified_within_days",operation.parameters.modifiedWithinDays}}.dump();
        }
        const auto strategyKey=strategyIdentity+"#guidance:"+std::to_string(recovery.hints.size());
        if(recovery.active && readOnly(operation) && recovery.tried.contains(strategyKey)) {
            if(++recovery.rejectedPlans<2 && recovery.canTry(recoveryPolicy_)) {s.state.addHistory("That unchanged strategy was already attempted. Choose a materially different safe operation or ask for guidance.");continue;}
            return askStrategyGuidance(id,"repeated_strategy");
        }
        if(!readOnly(operation) && s.successfulMutations[id].contains(key)) {
            const std::string message="A successful "+operation.name+" will not be replayed in the same step to repair incomplete verification. Re-observe state or supply additional evidence.";
            kernel.recordFailure(id,{RuntimeCause::EVIDENCE_UNAVAILABLE,"successful_mutation_replay",message,true});
            return finish(RuntimeOutcome::PARTIAL_SUCCESS,RuntimeCause::EVIDENCE_UNAVAILABLE,message);
        }
        if(source.postconditionKind=="file_content" && operation.name=="text.write") {
            if(reject("A new file cannot overwrite a collision through text.write; create it with its complete contents."))return finish(RuntimeOutcome::NEEDS_USER,RuntimeCause::ALREADY_EXISTS,"Artifact collision requires an explicit separate write request.");continue;
        }
        if(!source.postconditionKind.empty() && (operation.name=="filesystem.create" || operation.name=="filesystem.create_file")) {
            const auto expected=(source.artifactParentReference?Utf8::toPath(boundArtifactParent):artifactParent(source.artifactLocation))/Utf8::toPath(source.artifactName);
            if((source.postconditionKind=="directory_exists")!=(operation.name=="filesystem.create")) {
                if(reject("Creation primitive type differs from the declared artifact kind."))return finish(RuntimeOutcome::NEEDS_USER,RuntimeCause::VALIDATION_ERROR,"Artifact type mismatch.");continue;
            }
            const auto actual=artifactParent(operation.parameters.location)/Utf8::toPath(operation.parameters.name);
            if(!wf::sameIdentity(Utf8::path(expected),Utf8::path(actual))) {
                if(reject("Operation differs from structured artifact identity; preserve the requested name and existing parent."))return finish(RuntimeOutcome::NEEDS_USER,RuntimeCause::VALIDATION_ERROR,"Artifact identity mismatch.");continue;
            }
        }
        if(s.browserActive && operation.name.starts_with("keyboard.")) {
            if(reject("Controlled browser actions require observed DOM controls, not global keyboard input."))return finish(RuntimeOutcome::FAILED,RuntimeCause::VALIDATION_ERROR,"Repeated global keyboard action rejected.");continue;
        }
        if(recovery.active && !recovery.foregroundAllowed && (StrategyRecovery::foreground(operation.name) || recovery.foregroundSuggested && !readOnly(operation))) {
            PendingUserDecision pending;pending.questionId=s.identity+":"+std::to_string(++s.questionSequence);pending.stepId=id;
            pending.reason="USER_ACTION_REQUIRED";pending.intent="This recovery method can change the active window or use the global keyboard or mouse. May I take foreground control for this step?";
            pending.allowedActions={"GRANT_FOREGROUND","DECLINE_FOREGROUND"};pending.context=recovery.context(recoveryPolicy_);pending.context["permission_scope"]="current_step_and_guidance";
            pending.workflowState=ledger(kernel);s.pending=pending;s.heldDecision=decision;
            kernel.recordFailure(id,{RuntimeCause::USER_ACTION_REQUIRED,"foreground_permission",pending.intent,true});
            return finish(RuntimeOutcome::NEEDS_USER,RuntimeCause::USER_ACTION_REQUIRED,pending.intent);
        }
        if(!operation.parameters.elementId.empty() && s.staleReferences.contains(operation.parameters.elementId)) {
            if(reject("Discard stale element ID and choose a target from fresh observation."))return finish(RuntimeOutcome::NEEDS_USER,RuntimeCause::STALE_ELEMENT,"Repeated stale element reference.");continue;
        }
        if(s.noChange>=2 && (operation.name=="browser.click" || operation.name=="browser.type"))return finish(RuntimeOutcome::NEEDS_USER,RuntimeCause::NO_STATE_CHANGE,"Repeated browser actions have no observed semantic effect.");
        if(operation.name.starts_with("browser.") && !s.browserActive && operation.name!="browser.navigate" && operation.name!="browser.observe") {
            s.browserActive=true;s.state.addHistory("Observe browser state before advancing this READY step; choose a target from that observation.");continue;
        }
        if(operation.name.starts_with("browser."))s.browserActive=true;
        std::string error;
        if(!kernel.begin(id,error))return finish(RuntimeOutcome::FAILED,RuntimeCause::INVALID_PLAN,error);
        recordEvent("[OPERATION] step="+id+" "+operation.name+" "+decision.operationJson+"\n");result.executionStarted=true;
        if(recovery.guided)recoveryTransition(id,"ATTEMPTED","strategy_started");
        auto execution=execute(operation);
        result.operationResults.push_back({id,0,execution});result.operationResults.back().execution.browserData.clear();
        auto evidence=hostEvidence(id,operation,execution);
        const auto applicationFacts=operation.name=="application.observe"?evidence.facts:json::object();
        const auto evidenceId=kernel.facts.add(std::move(evidence)).id;
        if(recovery.attempts.size()<recoveryPolicy_.totalAttempts) {
            const auto* retained=kernel.facts.find(evidenceId);std::string outcome=execution.status==ExecutionStatus::SUCCESS?"observed_unverified":"execution_failed";
            if(operation.name=="filesystem.search" && execution.status==ExecutionStatus::SUCCESS && retained->facts.contains("results") && retained->facts["results"].is_array() && retained->facts["results"].empty()) {
                outcome="no_match";recovery.active=true;recovery.blocker="no_match";recovery.goalClass="filesystem_discovery";
            }
            json attempt={{"strategy",operation.name},{"outcome",outcome},{"mode",StrategyRecovery::inputMode(operation.name)},{"evidence_id",evidenceId}};
            if(proposed.contains("parameters"))for(const auto field:{"query","location","entry_type","extension","modified_within_days"})if(proposed["parameters"].contains(field))attempt["clues"][field]=proposed["parameters"][field];
            recovery.attempts.push_back(attempt);recovery.tried.insert(strategyKey);++recovery.sinceGuidance;
            if(readOnly(operation) && operation.name.starts_with("filesystem.") && execution.status==ExecutionStatus::SUCCESS) {
                const auto state=std::hash<std::string>{}(retained->facts.dump());
                const auto prior=recovery.readStates.find(key);
                if(prior!=recovery.readStates.end() && prior->second==state && !recovery.active) {recovery.active=true;recovery.blocker="equivalent_unverified_observation";}
                recovery.readStates[key]=state;
            }else if(!readOnly(operation) && execution.status==ExecutionStatus::SUCCESS) {recovery.tried.clear();recovery.readStates.clear();}
        }

        const bool existingDirectory=execution.postconditionVerified && execution.verifiedKind=="directory_exists" && execution.failureCategory=="already_exists";
        if(execution.status==ExecutionStatus::SUCCESS || existingDirectory) {
            kernel.operationFinished(id);s.invalidDecisions.erase(id);
            result.mappedPlan.steps.push_back(mapped.steps.front());s.state.addHistory(operation.name+" succeeded; structured evidence="+evidenceId);
            recordEvent("[EXECUTION] "+operation.name+" : SUCCESS evidence="+evidenceId+"\n");
            if(!readOnly(operation))s.successfulMutations[id].insert(key);
            if(operation.name=="filesystem.search") {
                const auto facts=kernel.facts.find(evidenceId)->facts;
                s.searchCandidates=discoveryCandidates(*kernel.facts.find(evidenceId),selected);s.searchDecisions[evidenceId]=decision;
                s.resolvedTarget.clear();
                if(!s.searchCandidates.empty() && !facts.value("clear_winner",false)) {
                    PendingUserDecision pending;pending.questionId=s.identity+":"+std::to_string(++s.questionSequence);pending.stepId=id;
                    pending.reason="TARGET_AMBIGUOUS";pending.intent="Which of these matching files did you mean?";
                    pending.candidates=s.searchCandidates;pending.searchEvidenceId=evidenceId;pending.workflowState=ledger(kernel);
                    pending.allowedActions={"SELECT_CANDIDATE","SHOW_CANDIDATES","REFINE_SEARCH"};s.pending=pending;s.heldDecision.reset();
                    kernel.recordFailure(id,{RuntimeCause::TARGET_AMBIGUOUS,"target_ambiguous",pending.intent,true});
                    return finish(RuntimeOutcome::NEEDS_USER,RuntimeCause::TARGET_AMBIGUOUS,pending.intent);
                }
                if(!s.searchCandidates.empty()) {
                    s.resolvedTarget=s.searchCandidates[0].value("path",std::string{});
                    wf::Evidence resolved;resolved.stepId=id;resolved.operation="filesystem.resolve";
                    resolved.facts={{"resolved_target",s.resolvedTarget},{"candidate_id",s.searchCandidates[0]["candidate_id"]},{"search_evidence_id",evidenceId},{"user_confirmed",false}};
                    kernel.facts.add(std::move(resolved));
                }
            }
            if(operation.name=="application.launch") {
                const auto& launch=kernel.facts.find(evidenceId)->facts;
                PrimitiveOperation observe{};observe.name="application.observe";observe.type=PrimitiveOperationType::APPLICATION_OBSERVE;observe.resolved=true;
                if(launch.contains("process_id"))observe.parameters.processId=std::to_string(ApplicationCorrelation::ticks(launch,"process_id"));
                else observe.parameters.application=launch.value("process_name",std::string{});
                observe.parameters.target=launch.value("target_path",operation.parameters.target);
                if(!observe.parameters.processId.empty() || !observe.parameters.application.empty())s.applicationScopes[id]=observe;
            }
            if(operation.name=="application.observe") {
                s.applicationScopes[id]=operation;
                const auto scope=id+":"+key;
                const auto fingerprint=ApplicationState::fingerprint(applicationFacts);
                const auto previous=s.applicationFingerprints.find(scope);
                const bool unchanged=previous!=s.applicationFingerprints.end() && previous->second==fingerprint;
                s.applicationFingerprints[scope]=fingerprint;
                s.lastApplicationFingerprint[id]=fingerprint;
                recordEvent("[APPLICATION_PROOF] "+applicationFacts.value("target_proof_level",std::string("unavailable"))+"\n");
                if(ApplicationState::targetUnavailable(applicationFacts) || unchanged) {
                    const std::string message=ApplicationState::targetUnavailable(applicationFacts)
                        ? "Application observation succeeded, but neither an exact document path nor sufficient launch/process/window correlation is available. Dispatch is retained; the document relationship remains unverified."
                        : "Equivalent application observations supplied no new evidence for this step. Verification is paused without replaying successful actions.";
                    kernel.recordFailure(id,{RuntimeCause::EVIDENCE_UNAVAILABLE,"application_evidence_unavailable",message,true});
                    result.recovery="Supply additional evidence or change the application state, then re-observe and continue.";
                    return finish(RuntimeOutcome::PARTIAL_SUCCESS,RuntimeCause::EVIDENCE_UNAVAILABLE,message);
                }
            }else if(!readOnly(operation)) {
                // A real state-changing action allows a fresh observation comparison.
                const auto prefix=id+":";
                for(auto it=s.applicationFingerprints.begin();it!=s.applicationFingerprints.end();)
                    if(it->first.starts_with(prefix))it=s.applicationFingerprints.erase(it);else ++it;
            }
            if(!source.postconditionKind.empty() || source.contentRequirement=="required" || source.contentRequirement=="empty_allowed") {
                if(!kernel.verify(id,{evidenceId},{},error))s.state.addHistory("Artifact postcondition not yet satisfied: "+error);
            }
            if(!execution.browserData.empty()) {
                const auto wire=BrowserState::snapshot(execution.browserData);
                if(wire.is_object() && wire.contains("effects")) {
                    wf::Evidence effects;effects.stepId=id;effects.operation=operation.name;effects.category=wf::EvidenceCategory::BROWSER_EFFECT;effects.facts={{"effects",wire["effects"]}};kernel.facts.add(std::move(effects));
                }
            }
        }else {
            kernel.recordFailure(id,{backendCause(execution.failureCategory),execution.failureCategory,execution.message,true});
            result.lastExecutionIssue=operation.name+" ["+execution.failureCategory+"]: "+execution.message;
            result.recovery="Re-observe and repair the affected READY step; verification clears its active recovery cause.";
            s.state.addHistory(result.lastExecutionIssue);recordEvent("[EXECUTION] "+operation.name+" : FAILED ["+execution.failureCategory+"]\n");
            if(execution.failureCategory=="already_exists")return finish(RuntimeOutcome::NEEDS_USER,RuntimeCause::ALREADY_EXISTS,execution.message);
            if(execution.failureCategory=="stale_element" && !operation.parameters.elementId.empty())s.staleReferences.insert(operation.parameters.elementId);
            if(kernel.graph.find(id)->recovery.attempts>=3)return askStrategyGuidance(id,"repeated_execution_failure");
        }
        if(operation.name=="browser.click" || operation.name=="browser.type") {
            s.lastBrowserAction=operation.name;s.beforeAction=s.lastFingerprint;
        }
        const auto declaredAfterOperation=evaluateDeclared(id);
        if(declaredAfterOperation==DeclaredState::EVIDENCE_UNAVAILABLE)return result;
        if(declaredAfterOperation!=DeclaredState::VERIFIED && recovery.active && !recovery.canTry(recoveryPolicy_))return askStrategyGuidance(id,recovery.blocker);
        announce();
    }
    if(const auto* step=kernel.next())return askStrategyGuidance(step->id,"planning_budget_exhausted");
    return finish(RuntimeOutcome::PARTIAL_SUCCESS,RuntimeCause::ITERATION_LIMIT,"Bounded workflow planning budget exhausted; verified progress is retained.");
}
