#pragma once
#include "predicate_registry.hpp"
#include "semantic_evidence.hpp"
#include "utf8_text.hpp"
#include <algorithm>

// Dynamic resources are inputs to a declared contract, never planner-provided
// verification claims. Binding produces a local copy, preserving graph/facts.
namespace EffectContract {
using nlohmann::json;
inline bool supported(const json& proof) {
    const auto source=proof.value("source",std::string{}),kind=PredicateRegistry::kindOf(proof);
    return source=="effect" && kind=="effect_achieved" || source=="application" && kind=="opened";
}
inline bool bind(json& proof,const workflow::WorkflowGraph& graph,const workflow::FactStore& facts,
                 const std::string& stepId,std::string& error,std::vector<std::string>* references=nullptr) {
    if(!supported(proof))return true;
    auto expected=PredicateRegistry::constraints(proof);const auto target=expected.value("target_path",std::string{});
    if(target!="$resolved_target" && target!="$resolved_parent") {
        if(target.starts_with('$')) {error="Unsupported dynamic effect target reference: "+target;return false;}
        return true;
    }
    auto scoped=[&](const workflow::Evidence& item) {
        const auto* producer=graph.find(item.stepId);
        return item.current && (item.stepId==stepId || producer && producer->status==workflow::StepStatus::VERIFIED && graph.ancestor(item.stepId,stepId));
    };
    std::string selected;
    for(const auto& item:facts.all()) {
        if(!scoped(item) || (item.operation!="filesystem.resolve" && item.operation!="user.select_candidate"))continue;
        const auto path=item.facts.value("resolved_target",std::string{});
        if(!Utf8::toPath(path).is_absolute())continue;
        const auto* search=facts.find(item.facts.value("search_evidence_id",std::string{}));
        if(!search || !scoped(*search) || search->operation!="filesystem.search" || !search->facts.value("operation_succeeded",false))continue;
        bool retained=false;
        for(const auto& row:search->facts.value("results",json::array()))
            retained |= row.value("candidate_id",std::string{})==item.facts.value("candidate_id",std::string{}) && SemanticEvidence::samePath(row.value("path",std::string{}),path);
        if(!retained)continue;
        if(!selected.empty() && !SemanticEvidence::samePath(selected,path)) {error="Ambiguous scoped resource for dynamic effect target";return false;}
        selected=path;
        if(references)for(const auto& id:{item.id,search->id})if(std::find(references->begin(),references->end(),id)==references->end())references->push_back(id);
    }
    if(selected.empty()) {error="No retained scoped canonical resource for "+target;return false;}
    expected["target_path"]=target=="$resolved_parent"?Utf8::path(Utf8::toPath(selected).parent_path()):selected;
    for(const auto& [field,type]:PredicateRegistry::fields(proof.at("source").get<std::string>()))proof.erase(field);
    proof["match"]=expected;return true;
}
}
