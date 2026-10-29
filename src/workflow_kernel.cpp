#include "artifact_binding.hpp"
#include "predicate_registry.hpp"
#include "workflow_kernel.hpp"
#include "semantic_evidence.hpp"
#include <algorithm>
#include <cctype>
#include <functional>
#include <stdexcept>

namespace workflow {
const char* statusName(StepStatus status) {
    switch(status) {
#define S(v) case StepStatus::v:return #v;
        S(PENDING) S(READY) S(RUNNING) S(VERIFIED) S(SKIPPED) S(RETRYABLE) S(BLOCKED) S(FAILED)
#undef S
    }
    return "FAILED";
}
bool sameIdentity(std::string a,std::string b) {
    auto normalize=[](std::string value) {
        std::replace(value.begin(),value.end(),'\\','/');
        std::string result;
        for(char c:value) if(c!='/' || result.empty() || result.back()!='/') result+=c;
        while(result.size()>1 && result.back()=='/') result.pop_back();
        if(result.size()>1 && result[1]==':')
            std::transform(result.begin(),result.end(),result.begin(),[](unsigned char c){return std::tolower(c);});
        return result;
    };
    return normalize(std::move(a))==normalize(std::move(b));
}
const Evidence& FactStore::add(Evidence evidence) {
    if(!evidence.facts.is_object()) throw std::invalid_argument("Evidence facts must be an object");
    if(evidence.id.empty()) do {evidence.id="evidence_"+std::to_string(++sequence_);} while(ids_.contains(evidence.id));
    if(!ids_.insert(evidence.id).second) throw std::invalid_argument("Duplicate evidence ID: "+evidence.id);
    if(evidence.category==EvidenceCategory::BROWSER_OBSERVATION)
        for(auto& old:evidence_) if(old.category==EvidenceCategory::BROWSER_OBSERVATION) old.current=false;
    if(evidence.category==EvidenceCategory::APPLICATION_OBSERVATION ||
       (evidence.category==EvidenceCategory::APPLICATION_DISPATCH && evidence.facts.value("operation_succeeded",false)))
        for(auto& old:evidence_) if(old.category==EvidenceCategory::APPLICATION_OBSERVATION) old.current=false;
    // Same-step search refinement supersedes discovery/selection currency, never its immutable facts.
    if(evidence.operation=="filesystem.search" && evidence.facts.value("operation_succeeded",false)) {
        std::set<std::string> superseded;
        for(auto& old:evidence_)if(old.stepId==evidence.stepId && old.operation=="filesystem.search") {old.current=false;superseded.insert(old.id);}
        for(auto& old:evidence_)if(old.facts.contains("search_evidence_id") && old.facts["search_evidence_id"].is_string() &&
            superseded.contains(old.facts["search_evidence_id"].get<std::string>()))old.current=false;
    }
    evidence_.push_back(std::move(evidence));return evidence_.back();
}
const Evidence* FactStore::find(const std::string& id) const {
    for(const auto& item:evidence_) if(item.id==id) return &item;
    return nullptr;
}
const Evidence* FactStore::latestFact(const std::string& stepId,const std::string& field) const {
    for(auto it=evidence_.rbegin();it!=evidence_.rend();++it)
        if(it->stepId==stepId && it->current && it->category!=EvidenceCategory::FAILURE && it->facts.contains(field)) return &*it;
    return nullptr;
}
WorkflowGraph::WorkflowGraph(std::vector<Step> steps):steps_(std::move(steps)) {
    if(steps_.empty()) {error_="Workflow contains no steps";return;}
    for(auto& step:steps_) if(step.postconditions.empty()) {
        Postcondition expected;expected.kind=PredicateKind::INTERPRETED;step.postconditions.push_back(expected);
    }
    // Admission validates semantic contracts before any scheduler/operation work.
    for(const auto& step:steps_)for(const auto& post:step.postconditions) {
        std::string detail;
        if(post.kind==PredicateKind::SEMANTIC && SemanticEvidence::compile(post.value,detail).empty()) {error_="Invalid postcondition for "+step.id+": "+detail;return;}
        if(post.kind==PredicateKind::ARTIFACT && (!post.value.is_string() || !PredicateRegistry::artifactKinds.contains(post.value.get<std::string>()))) {error_="Unsupported artifact postcondition for "+step.id;return;}
        switch(post.kind) {
            case PredicateKind::SEMANTIC:case PredicateKind::ARTIFACT:case PredicateKind::INTERPRETED:case PredicateKind::FACT_EQUALS:case PredicateKind::FACT_CONTAINS:break;
            default:error_="Unsupported postcondition kind for "+step.id;return;
        }
    }
    std::set<std::string> ids;
    for(const auto& step:steps_) if(step.id.empty() || !ids.insert(step.id).second) {error_="Empty or duplicate step ID: "+step.id;return;}
    for(const auto& step:steps_) {
        std::set<std::string> dependencies;
        for(const auto& dep:step.dependencies) if(dep==step.id || !ids.contains(dep) || !dependencies.insert(dep).second) {
            error_="Invalid dependency "+dep+" for "+step.id;return;
        }
    }
    std::map<std::string,int> color;
    std::function<bool(const Step&)> visit=[&](const Step& step) {
        if(color[step.id]==1) return false;
        if(color[step.id]==2) return true;
        color[step.id]=1;
        for(const auto& dep:step.dependencies) if(!visit(*find(dep))) return false;
        color[step.id]=2;return true;
    };
    for(const auto& step:steps_) if(!visit(step)) {error_="Cyclic workflow at "+step.id;return;}
    for(const auto& step:steps_) if(step.condition && (step.condition->field.empty() || !step.condition->equals.is_primitive() || !ancestor(step.condition->stepId,step.id))) {
        error_="Condition must reference an ancestor fact for "+step.id;return;
    }
    for(const auto& step:steps_)for(const auto& post:step.postconditions)if(!post.pathFromStep.empty()) {
        const auto* producer=find(post.pathFromStep);bool directory=false;
        if(producer)for(const auto& target:producer->postconditions)directory |= target.kind==PredicateKind::ARTIFACT && target.value=="directory_exists";
        if(post.kind!=PredicateKind::ARTIFACT || !post.path.empty() || !ArtifactBinding::leaf(post.artifactLeaf) || !directory || !ancestor(post.pathFromStep,step.id)) {error_="Invalid produced-artifact parent reference for "+step.id;return;}
    }

}
Step* WorkflowGraph::find(const std::string& id) {for(auto& step:steps_) if(step.id==id) return &step;return nullptr;}
const Step* WorkflowGraph::find(const std::string& id) const {for(const auto& step:steps_) if(step.id==id) return &step;return nullptr;}
bool WorkflowGraph::ancestor(const std::string& prerequisite,const std::string& descendant) const {
    const auto* step=find(descendant);if(!step) return false;
    std::set<std::string> seen;auto pending=step->dependencies;
    while(!pending.empty()) {
        auto id=pending.back();pending.pop_back();if(!seen.insert(id).second) continue;
        if(id==prerequisite) return true;
        if(const auto* dep=find(id)) pending.insert(pending.end(),dep->dependencies.begin(),dep->dependencies.end());
    }
    return false;
}
bool WorkflowGraph::dependenciesSatisfied(const Step& step) const {
    for(const auto& id:step.dependencies) {
        const auto* dep=find(id);
        if(!dep || (dep->status!=StepStatus::VERIFIED && !(dep->status==StepStatus::SKIPPED && dep->validSkip))) return false;
    }
    return true;
}
bool WorkflowGraph::complete() const {
    return valid() && std::all_of(steps_.begin(),steps_.end(),[](const Step& step) {
        return !step.required || step.status==StepStatus::VERIFIED || (step.status==StepStatus::SKIPPED && step.validSkip);
    });
}
Truth WorkflowScheduler::evaluate(const Condition& condition,const WorkflowGraph& graph,const FactStore& facts) {
    const auto* producer=graph.find(condition.stepId);
    if(!producer || producer->status!=StepStatus::VERIFIED) return Truth::UNKNOWN;
    const auto* evidence=facts.latestFact(condition.stepId,condition.field);
    if(!evidence) return Truth::UNKNOWN;
    return evidence->facts[condition.field]==condition.equals?Truth::TRUE_VALUE:Truth::FALSE_VALUE;
}
void WorkflowScheduler::refresh(WorkflowGraph& graph,const FactStore& facts) {
    if(!graph.valid()) return;
    // Fixed point is needed when false branches unlock a join regardless of vector order.
    for(size_t pass=0;pass<=graph.steps().size();++pass) {
        bool changed=false;
        for(const auto& item:graph.steps()) {
            auto& step=*graph.find(item.id);
            if(step.status==StepStatus::VERIFIED || step.status==StepStatus::SKIPPED || step.status==StepStatus::RUNNING || step.status==StepStatus::FAILED) continue;
            if(step.recovery.active && !step.recovery.active->retryable) {step.status=StepStatus::BLOCKED;continue;}
            const auto old=step.status;
            if(!graph.dependenciesSatisfied(step)) step.status=StepStatus::PENDING;
            else if(step.condition) {
                const auto truth=evaluate(*step.condition,graph,facts);
                if(truth==Truth::FALSE_VALUE) {step.status=StepStatus::SKIPPED;step.validSkip=true;step.recovery.verified();}
                else step.status=truth==Truth::TRUE_VALUE?StepStatus::READY:StepStatus::BLOCKED;
            } else step.status=StepStatus::READY;
            changed |= old!=step.status;
        }
        if(!changed) break;
    }
}
const Step* WorkflowScheduler::next(const WorkflowGraph& graph) {for(const auto& step:graph.steps()) if(step.status==StepStatus::READY) return &step;return nullptr;}
bool WorkflowScheduler::begin(WorkflowGraph& graph,const FactStore& facts,const std::string& id,std::string& error) {
    auto* step=graph.find(id);
    if(!graph.valid() || !step || step->status!=StepStatus::READY || !graph.dependenciesSatisfied(*step) ||
       (step->condition && evaluate(*step->condition,graph,facts)!=Truth::TRUE_VALUE)) {
        error="Step is not READY: "+id;return false;
    }
    step->status=StepStatus::RUNNING;return true;
}
std::string WorkflowScheduler::blocker(const WorkflowGraph& graph) {
    if(!graph.valid()) return graph.error();
    for(const auto& step:graph.steps()) {
        if(!step.required || step.status==StepStatus::VERIFIED || (step.status==StepStatus::SKIPPED && step.validSkip)) continue;
        if(step.recovery.active) return step.id+": "+step.recovery.active->code+": "+step.recovery.active->message;
        for(const auto& dep:step.dependencies) {
            const auto* source=graph.find(dep);
            if(!source || (source->status!=StepStatus::VERIFIED && !(source->status==StepStatus::SKIPPED && source->validSkip))) return step.id+": waiting for "+dep;
        }
        if(step.condition) return step.id+": condition UNKNOWN: "+step.condition->stepId+"."+step.condition->field;
        return step.id+": "+statusName(step.status);
    }
    return "No required work remains";
}
bool PostconditionVerifier::verify(WorkflowGraph& graph,const FactStore& facts,const std::string& stepId,
    const std::vector<std::string>& evidenceIds,const std::vector<Postcondition>& interpretation,std::string& error) {
    auto* step=graph.find(stepId);
    if(!step || !graph.dependenciesSatisfied(*step) || (step->status!=StepStatus::READY && step->status!=StepStatus::RUNNING && step->status!=StepStatus::RETRYABLE)) {error="Verification requires a runnable step: "+stepId;return false;}
    if(step->condition && WorkflowScheduler::evaluate(*step->condition,graph,facts)!=Truth::TRUE_VALUE) {error="Condition is not true";return false;}
    const bool interpreted=step->postconditions.size()==1 && step->postconditions.front().kind==PredicateKind::INTERPRETED;
    const auto& predicates=interpreted?interpretation:step->postconditions;
    if(predicates.empty() || evidenceIds.empty()) {error="Structured postconditions and evidence are required";return false;}
    std::vector<const Evidence*> usable;
    for(const auto& id:evidenceIds) {
        const auto* evidence=facts.find(id);
        if(!evidence || !evidence->current || evidence->category==EvidenceCategory::FAILURE) continue;
        const auto* producer=graph.find(evidence->stepId);
        if(evidence->stepId==stepId || (producer && producer->status==StepStatus::VERIFIED && graph.ancestor(evidence->stepId,stepId))) usable.push_back(evidence);
    }
    std::vector<std::string> supportingIds;
    for(const auto& predicate:predicates) {
        bool matched=false;auto expectedPath=predicate.path;
        if(!predicate.pathFromStep.empty()) {
            const auto parent=ArtifactBinding::parent(graph,facts,stepId,predicate.pathFromStep);
            if(!parent.okay){error=parent.error;return false;}
            expectedPath=Utf8::path((Utf8::toPath(parent.path)/Utf8::toPath(predicate.artifactLeaf)).lexically_normal());
        }
        for(const auto* item:usable) {
            if(!predicate.operation.empty() && item->operation!=predicate.operation) continue;
            if(!expectedPath.empty() && (!item->facts.contains("path") || !item->facts["path"].is_string() || !sameIdentity(item->facts["path"].get<std::string>(),expectedPath))) continue;
            if(predicate.kind==PredicateKind::SEMANTIC) {std::string detail;matched=SemanticEvidence::matches(*item,predicate.value,detail);}
            else if(predicate.kind==PredicateKind::ARTIFACT) matched=item->category==EvidenceCategory::ARTIFACT && Json(item->facts.value("verified_kind",std::string{}))==predicate.value;
            else if(item->facts.contains(predicate.field)) {
                const auto& value=item->facts[predicate.field];
                if(predicate.kind==PredicateKind::FACT_EQUALS) matched=value==predicate.value;
                else if(value.is_array()) matched=std::find(value.begin(),value.end(),predicate.value)!=value.end();
                else matched=value.is_string() && predicate.value.is_string() && !predicate.value.get<std::string>().empty() && value.get<std::string>().find(predicate.value.get<std::string>())!=std::string::npos;
            }
            if(matched) {if(std::find(supportingIds.begin(),supportingIds.end(),item->id)==supportingIds.end())supportingIds.push_back(item->id);break;}
        }
        if(!matched) {error="Postcondition unsupported by scoped structured evidence for "+stepId;
            if(predicate.kind==PredicateKind::SEMANTIC)error+=". "+SemanticEvidence::diagnostic(predicate.value,usable);return false;}
    }
    step->status=StepStatus::VERIFIED;step->evidenceIds=std::move(supportingIds);step->recovery.verified();return true;
}
void Kernel::recordFailure(const std::string& id,RecoveryCause cause) {
    auto* step=graph.find(id);if(!step) throw std::invalid_argument("Unknown failed step: "+id);
    if(step->status==StepStatus::VERIFIED || step->status==StepStatus::SKIPPED) throw std::logic_error("Terminal step cannot fail");
    const bool retryable=cause.retryable;step->recovery.fail(std::move(cause));step->status=retryable?StepStatus::RETRYABLE:StepStatus::BLOCKED;
}
void Kernel::operationFinished(const std::string& id) {auto* step=graph.find(id);if(!step || step->status!=StepStatus::RUNNING) throw std::logic_error("No running operation for "+id);step->status=StepStatus::READY;}
bool Kernel::verify(const std::string& id,const std::vector<std::string>& ids,const std::vector<Postcondition>& interpretation,std::string& error) {
    const bool result=PostconditionVerifier::verify(graph,facts,id,ids,interpretation,error);if(result) refresh();return result;
}
void Kernel::retry() {
    for(const auto& item:graph.steps()) if(auto* step=graph.find(item.id);step->recovery.active && step->recovery.active->retryable) step->status=StepStatus::RETRYABLE;
    refresh();
}
}
