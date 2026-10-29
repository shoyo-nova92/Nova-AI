#pragma once
#include "runtime_outcome.hpp"
#include <nlohmann/json.hpp>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace workflow {
using Json=nlohmann::json;
enum class StepStatus { PENDING, READY, RUNNING, VERIFIED, SKIPPED, RETRYABLE, BLOCKED, FAILED };
enum class Truth { TRUE_VALUE, FALSE_VALUE, UNKNOWN };
enum class EvidenceCategory { OBSERVATION, ARTIFACT, BROWSER_OBSERVATION, BROWSER_EFFECT, APPLICATION_EFFECT, FAILURE, APPLICATION_DISPATCH, APPLICATION_OBSERVATION };
enum class PredicateKind { FACT_EQUALS, FACT_CONTAINS, ARTIFACT, INTERPRETED, SEMANTIC };
struct RecoveryCause {
    RuntimeCause category=RuntimeCause::UNKNOWN;
    std::string code, message;
    bool retryable=false;
};
struct StepRecoveryState {
    std::optional<RecoveryCause> active;
    unsigned attempts=0;
    void fail(RecoveryCause cause) {active=std::move(cause);++attempts;}
    void verified() {active.reset();}
};
struct Condition {std::string stepId, field;Json equals;};
struct Postcondition {
    PredicateKind kind=PredicateKind::FACT_EQUALS;
    std::string operation, path, field;
    Json value;
    std::string pathFromStep, artifactLeaf;
};
struct Evidence {
    std::string id, stepId, operation;
    EvidenceCategory category=EvidenceCategory::OBSERVATION;
    Json facts=Json::object();
    bool current=true;
};
struct Step {
    std::string id, intent;
    std::vector<std::string> dependencies;
    std::optional<Condition> condition;
    std::vector<Postcondition> postconditions;
    bool required=true;
    StepStatus status=StepStatus::PENDING;
    bool validSkip=false;
    std::vector<std::string> evidenceIds;
    StepRecoveryState recovery;
};
class FactStore {
public:
    const Evidence& add(Evidence evidence);
    const Evidence* find(const std::string& id) const;
    const std::vector<Evidence>& all() const {return evidence_;}
    const Evidence* latestFact(const std::string& stepId,const std::string& field) const;
private:
    std::vector<Evidence> evidence_;
    std::set<std::string> ids_;
    unsigned sequence_=0;
};
class WorkflowGraph {
public:
    explicit WorkflowGraph(std::vector<Step> steps);
    bool valid() const {return error_.empty();}
    const std::string& error() const {return error_;}
    const std::vector<Step>& steps() const {return steps_;}
    Step* find(const std::string& id);
    const Step* find(const std::string& id) const;
    bool ancestor(const std::string& prerequisite,const std::string& descendant) const;
    bool dependenciesSatisfied(const Step& step) const;
    bool complete() const;
private:
    std::vector<Step> steps_;
    std::string error_;
};
class WorkflowScheduler {
public:
    static Truth evaluate(const Condition& condition,const WorkflowGraph& graph,const FactStore& facts);
    static void refresh(WorkflowGraph& graph,const FactStore& facts);
    static const Step* next(const WorkflowGraph& graph);
    static bool begin(WorkflowGraph& graph,const FactStore& facts,const std::string& id,std::string& error);
    static std::string blocker(const WorkflowGraph& graph);
};
class PostconditionVerifier {
public:
    static bool verify(WorkflowGraph& graph,const FactStore& facts,const std::string& stepId,
                       const std::vector<std::string>& evidenceIds,
                       const std::vector<Postcondition>& interpretation,std::string& error);
};
class Kernel {
public:
    explicit Kernel(std::vector<Step> steps):graph(std::move(steps)) {}
    WorkflowGraph graph;
    FactStore facts;
    void refresh() {WorkflowScheduler::refresh(graph,facts);}
    const Step* next() {refresh();return WorkflowScheduler::next(graph);}
    bool begin(const std::string& id,std::string& error) {refresh();return WorkflowScheduler::begin(graph,facts,id,error);}
    void recordFailure(const std::string& id,RecoveryCause cause);
    void operationFinished(const std::string& id);
    bool verify(const std::string& id,const std::vector<std::string>& ids,const std::vector<Postcondition>& interpretation,std::string& error);
    bool complete() const {return graph.complete();}
    void retry();
};
const char* statusName(StepStatus status);
bool sameIdentity(std::string left,std::string right);
}
